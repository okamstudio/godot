/**************************************************************************/
/*  gdscript_tokenizer.cpp                                                */
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

#include "gdscript_tokenizer.h"

#include "core/error/error_macros.h"
#include "core/string/char_utils.h"

#ifdef DEBUG_ENABLED
#include "servers/text_server.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

static const char *token_names[] = {
	"Empty", // EMPTY,
	// Basic
	"Annotation", // ANNOTATION
	"Identifier", // IDENTIFIER,
	"Literal", // LITERAL,
	// Comparison
	"<", // LESS,
	"<=", // LESS_EQUAL,
	">", // GREATER,
	">=", // GREATER_EQUAL,
	"==", // EQUAL_EQUAL,
	"!=", // BANG_EQUAL,
	// Logical
	"and", // AND,
	"or", // OR,
	"not", // NOT,
	"&&", // AMPERSAND_AMPERSAND,
	"||", // PIPE_PIPE,
	"!", // BANG,
	// Bitwise
	"&", // AMPERSAND,
	"|", // PIPE,
	"~", // TILDE,
	"^", // CARET,
	"<<", // LESS_LESS,
	">>", // GREATER_GREATER,
	// Math
	"+", // PLUS,
	"-", // MINUS,
	"*", // STAR,
	"**", // STAR_STAR,
	"/", // SLASH,
	"%", // PERCENT,
	// Assignment
	"=", // EQUAL,
	"+=", // PLUS_EQUAL,
	"-=", // MINUS_EQUAL,
	"*=", // STAR_EQUAL,
	"**=", // STAR_STAR_EQUAL,
	"/=", // SLASH_EQUAL,
	"%=", // PERCENT_EQUAL,
	"<<=", // LESS_LESS_EQUAL,
	">>=", // GREATER_GREATER_EQUAL,
	"&=", // AMPERSAND_EQUAL,
	"|=", // PIPE_EQUAL,
	"^=", // CARET_EQUAL,
	// Control flow
	"if", // IF,
	"elif", // ELIF,
	"else", // ELSE,
	"for", // FOR,
	"while", // WHILE,
	"break", // BREAK,
	"continue", // CONTINUE,
	"pass", // PASS,
	"return", // RETURN,
	"match", // MATCH,
	"when", // WHEN,
	// Keywords
	"as", // AS,
	"assert", // ASSERT,
	"await", // AWAIT,
	"breakpoint", // BREAKPOINT,
	"class", // CLASS,
	"class_name", // CLASS_NAME,
	"const", // CONST,
	"enum", // ENUM,
	"extends", // EXTENDS,
	"func", // FUNC,
	"in", // IN,
	"is", // IS,
	"namespace", // NAMESPACE
	"preload", // PRELOAD,
	"self", // SELF,
	"signal", // SIGNAL,
	"static", // STATIC,
	"super", // SUPER,
	"trait", // TRAIT,
	"var", // VAR,
	"void", // VOID,
	"yield", // YIELD,
	// Punctuation
	"[", // BRACKET_OPEN,
	"]", // BRACKET_CLOSE,
	"{", // BRACE_OPEN,
	"}", // BRACE_CLOSE,
	"(", // PARENTHESIS_OPEN,
	")", // PARENTHESIS_CLOSE,
	",", // COMMA,
	";", // SEMICOLON,
	".", // PERIOD,
	"..", // PERIOD_PERIOD,
	":", // COLON,
	"$", // DOLLAR,
	"->", // FORWARD_ARROW,
	"_", // UNDERSCORE,
	// Whitespace
	"Newline", // NEWLINE,
	"Indent", // INDENT,
	"Dedent", // DEDENT,
	// Constants
	"PI", // CONST_PI,
	"TAU", // CONST_TAU,
	"INF", // CONST_INF,
	"NaN", // CONST_NAN,
	// Error message improvement
	"VCS conflict marker", // VCS_CONFLICT_MARKER,
	"`", // BACKTICK,
	"?", // QUESTION_MARK,
	// Special
	"Error", // ERROR,
	"End of file", // EOF,
};

// Avoid desync.
static_assert(sizeof(token_names) / sizeof(token_names[0]) == GDScriptTokenizer::Token::TK_MAX, "Amount of token names don't match the amount of token types.");

const char *GDScriptTokenizer::Token::get_name() const {
	ERR_FAIL_INDEX_V_MSG(type, TK_MAX, "<error>", "Using token type out of the enum.");
	return token_names[type];
}

String GDScriptTokenizer::Token::get_debug_name() const {
	switch (type) {
		case IDENTIFIER:
			return vformat(R"(identifier "%s")", source);
		default:
			return vformat(R"("%s")", get_name());
	}
}

bool GDScriptTokenizer::Token::can_precede_bin_op() const {
	switch (type) {
		case IDENTIFIER:
		case LITERAL:
		case SELF:
		case BRACKET_CLOSE:
		case BRACE_CLOSE:
		case PARENTHESIS_CLOSE:
		case CONST_PI:
		case CONST_TAU:
		case CONST_INF:
		case CONST_NAN:
			return true;
		default:
			return false;
	}
}

bool GDScriptTokenizer::Token::is_identifier() const {
	// Note: Most keywords should not be recognized as identifiers.
	// These are only exceptions for stuff that already is on the engine's API.
	switch (type) {
		case IDENTIFIER:
		case MATCH: // Used in String.match().
		case WHEN: // New keyword, avoid breaking existing code.
		// Allow constants to be treated as regular identifiers.
		case CONST_PI:
		case CONST_INF:
		case CONST_NAN:
		case CONST_TAU:
			return true;
		default:
			return false;
	}
}

bool GDScriptTokenizer::Token::is_node_name() const {
	// This is meant to allow keywords with the $ notation, but not as general identifiers.
	switch (type) {
		case IDENTIFIER:
		case AND:
		case AS:
		case ASSERT:
		case AWAIT:
		case BREAK:
		case BREAKPOINT:
		case CLASS_NAME:
		case CLASS:
		case CONST:
		case CONST_PI:
		case CONST_INF:
		case CONST_NAN:
		case CONST_TAU:
		case CONTINUE:
		case ELIF:
		case ELSE:
		case ENUM:
		case EXTENDS:
		case FOR:
		case FUNC:
		case IF:
		case IN:
		case IS:
		case MATCH:
		case NAMESPACE:
		case NOT:
		case OR:
		case PASS:
		case PRELOAD:
		case RETURN:
		case SELF:
		case SIGNAL:
		case STATIC:
		case SUPER:
		case TRAIT:
		case UNDERSCORE:
		case VAR:
		case VOID:
		case WHILE:
		case WHEN:
		case YIELD:
			return true;
		default:
			return false;
	}
}

String GDScriptTokenizer::get_token_name(Token::Type p_token_type) {
	ERR_FAIL_INDEX_V_MSG(p_token_type, Token::TK_MAX, "<error>", "Using token type out of the enum.");
	return token_names[p_token_type];
}

void GDScriptTokenizerText::set_source_code(const String &p_source_code) {
	source = p_source_code;
	if (source.is_empty()) {
		_source = U"";
	} else {
		_source = source.ptr();
	}
	_current = _source;
	line = 1;
	column = 1;
	length = p_source_code.length();
	position = 0;
}

void GDScriptTokenizerText::set_cursor_position(int p_line, int p_column) {
	cursor_line = p_line;
	cursor_column = p_column;
}

void GDScriptTokenizerText::set_multiline_mode(bool p_state) {
	multiline_mode = p_state;
}

void GDScriptTokenizerText::push_expression_indented_block() {
	indent_stack_stack.push_back(indent_stack);
}

void GDScriptTokenizerText::pop_expression_indented_block() {
	ERR_FAIL_COND(indent_stack_stack.is_empty());
	indent_stack = indent_stack_stack.back()->get();
	indent_stack_stack.pop_back();
}

int GDScriptTokenizerText::get_cursor_line() const {
	return cursor_line;
}

int GDScriptTokenizerText::get_cursor_column() const {
	return cursor_column;
}

bool GDScriptTokenizerText::is_past_cursor() const {
	if (line < cursor_line) {
		return false;
	}
	if (line > cursor_line) {
		return true;
	}
	if (column < cursor_column) {
		return false;
	}
	return true;
}

char32_t GDScriptTokenizerText::_advance() {
	if (unlikely(_is_at_end())) {
		return '\0';
	}
	_current++;
	column++;
	position++;
	if (column > rightmost_column) {
		rightmost_column = column;
	}
	if (unlikely(_is_at_end())) {
		// Add extra newline even if it's not there, to satisfy the parser.
		newline(true);
		// Also add needed unindent.
		check_indent();
	}
	return _peek(-1);
}

void GDScriptTokenizerText::push_paren(char32_t p_char) {
	paren_stack.push_back(p_char);
}

bool GDScriptTokenizerText::pop_paren(char32_t p_expected) {
	if (paren_stack.is_empty()) {
		return false;
	}
	char32_t actual = paren_stack.back()->get();
	paren_stack.pop_back();

	return actual == p_expected;
}

GDScriptTokenizer::Token GDScriptTokenizerText::pop_error() {
	Token error = error_stack.back()->get();
	error_stack.pop_back();
	return error;
}

GDScriptTokenizer::Token GDScriptTokenizerText::make_token(Token::Type p_type) {
	Token token(p_type);
	token.start_line = start_line;
	token.end_line = line;
	token.start_column = start_column;
	token.end_column = column;
	token.leftmost_column = leftmost_column;
	token.rightmost_column = rightmost_column;
	token.source = String(_start, _current - _start);

	if (p_type != Token::ERROR && cursor_line > -1) {
		// Also count whitespace after token.
		int offset = 0;
		while (_peek(offset) == ' ' || _peek(offset) == '\t') {
			offset++;
		}
		int last_column = column + offset;
		// Check cursor position in token.
		if (start_line == line) {
			// Single line token.
			if (cursor_line == start_line && cursor_column >= start_column && cursor_column <= last_column) {
				token.cursor_position = cursor_column - start_column;
				if (cursor_column == start_column) {
					token.cursor_place = CURSOR_BEGINNING;
				} else if (cursor_column < column) {
					token.cursor_place = CURSOR_MIDDLE;
				} else {
					token.cursor_place = CURSOR_END;
				}
			}
		} else {
			// Multi line token.
			if (cursor_line == start_line && cursor_column >= start_column) {
				// Is in first line.
				token.cursor_position = cursor_column - start_column;
				if (cursor_column == start_column) {
					token.cursor_place = CURSOR_BEGINNING;
				} else {
					token.cursor_place = CURSOR_MIDDLE;
				}
			} else if (cursor_line == line && cursor_column <= last_column) {
				// Is in last line.
				token.cursor_position = cursor_column - start_column;
				if (cursor_column < column) {
					token.cursor_place = CURSOR_MIDDLE;
				} else {
					token.cursor_place = CURSOR_END;
				}
			} else if (cursor_line > start_line && cursor_line < line) {
				// Is in middle line.
				token.cursor_position = CURSOR_MIDDLE;
			}
		}
	}

	last_token = token;
	return token;
}

GDScriptTokenizer::Token GDScriptTokenizerText::make_literal(const Variant &p_literal) {
	Token token = make_token(Token::LITERAL);
	token.literal = p_literal;
	return token;
}

GDScriptTokenizer::Token GDScriptTokenizerText::make_identifier(const StringName &p_identifier) {
	Token identifier = make_token(Token::IDENTIFIER);
	identifier.literal = p_identifier;
	return identifier;
}

GDScriptTokenizer::Token GDScriptTokenizerText::make_error(const String &p_message) {
	Token error = make_token(Token::ERROR);
	error.literal = p_message;

	return error;
}

void GDScriptTokenizerText::push_error(const String &p_message) {
	Token error = make_error(p_message);
	error_stack.push_back(error);
}

void GDScriptTokenizerText::push_error(const Token &p_error) {
	error_stack.push_back(p_error);
}

GDScriptTokenizer::Token GDScriptTokenizerText::make_paren_error(char32_t p_paren) {
	if (paren_stack.is_empty()) {
		return make_error(vformat("Closing \"%c\" doesn't have an opening counterpart.", p_paren));
	}
	Token error = make_error(vformat("Closing \"%c\" doesn't match the opening \"%c\".", p_paren, paren_stack.back()->get()));
	paren_stack.pop_back(); // Remove opening one anyway.
	return error;
}

GDScriptTokenizer::Token GDScriptTokenizerText::check_vcs_marker(char32_t p_test, Token::Type p_double_type) {
	const char32_t *next = _current + 1;
	int chars = 2; // Two already matched.

	// Test before consuming characters, since we don't want to consume more than needed.
	while (*next == p_test) {
		chars++;
		next++;
	}
	if (chars >= 7) {
		// It is a VCS conflict marker.
		while (chars > 1) {
			// Consume all characters (first was already consumed by scan()).
			_advance();
			chars--;
		}
		return make_token(Token::VCS_CONFLICT_MARKER);
	} else {
		// It is only a regular double character token, so we consume the second character.
		_advance();
		return make_token(p_double_type);
	}
}

GDScriptTokenizer::Token GDScriptTokenizerText::annotation() {
	if (is_unicode_identifier_start(_peek())) {
		_advance(); // Consume start character.
	} else {
		push_error("Expected annotation identifier after \"@\".");
	}
	while (is_unicode_identifier_continue(_peek())) {
		// Consume all identifier characters.
		_advance();
	}
	Token annotation = make_token(Token::ANNOTATION);
	annotation.literal = StringName(annotation.source);
	return annotation;
}

#define KEYWORDS(KEYWORD_GROUP, KEYWORD)     \
	KEYWORD_GROUP('a')                       \
	KEYWORD("as", Token::AS)                 \
	KEYWORD("and", Token::AND)               \
	KEYWORD("assert", Token::ASSERT)         \
	KEYWORD("await", Token::AWAIT)           \
	KEYWORD_GROUP('b')                       \
	KEYWORD("break", Token::BREAK)           \
	KEYWORD("breakpoint", Token::BREAKPOINT) \
	KEYWORD_GROUP('c')                       \
	KEYWORD("class", Token::CLASS)           \
	KEYWORD("class_name", Token::CLASS_NAME) \
	KEYWORD("const", Token::CONST)           \
	KEYWORD("continue", Token::CONTINUE)     \
	KEYWORD_GROUP('e')                       \
	KEYWORD("elif", Token::ELIF)             \
	KEYWORD("else", Token::ELSE)             \
	KEYWORD("enum", Token::ENUM)             \
	KEYWORD("extends", Token::EXTENDS)       \
	KEYWORD_GROUP('f')                       \
	KEYWORD("for", Token::FOR)               \
	KEYWORD("func", Token::FUNC)             \
	KEYWORD_GROUP('i')                       \
	KEYWORD("if", Token::IF)                 \
	KEYWORD("in", Token::IN)                 \
	KEYWORD("is", Token::IS)                 \
	KEYWORD_GROUP('m')                       \
	KEYWORD("match", Token::MATCH)           \
	KEYWORD_GROUP('n')                       \
	KEYWORD("namespace", Token::NAMESPACE)   \
	KEYWORD("not", Token::NOT)               \
	KEYWORD_GROUP('o')                       \
	KEYWORD("or", Token::OR)                 \
	KEYWORD_GROUP('p')                       \
	KEYWORD("pass", Token::PASS)             \
	KEYWORD("preload", Token::PRELOAD)       \
	KEYWORD_GROUP('r')                       \
	KEYWORD("return", Token::RETURN)         \
	KEYWORD_GROUP('s')                       \
	KEYWORD("self", Token::SELF)             \
	KEYWORD("signal", Token::SIGNAL)         \
	KEYWORD("static", Token::STATIC)         \
	KEYWORD("super", Token::SUPER)           \
	KEYWORD_GROUP('t')                       \
	KEYWORD("trait", Token::TRAIT)           \
	KEYWORD_GROUP('v')                       \
	KEYWORD("var", Token::VAR)               \
	KEYWORD("void", Token::VOID)             \
	KEYWORD_GROUP('w')                       \
	KEYWORD("while", Token::WHILE)           \
	KEYWORD("when", Token::WHEN)             \
	KEYWORD_GROUP('y')                       \
	KEYWORD("yield", Token::YIELD)           \
	KEYWORD_GROUP('I')                       \
	KEYWORD("INF", Token::CONST_INF)         \
	KEYWORD_GROUP('N')                       \
	KEYWORD("NAN", Token::CONST_NAN)         \
	KEYWORD_GROUP('P')                       \
	KEYWORD("PI", Token::CONST_PI)           \
	KEYWORD_GROUP('T')                       \
	KEYWORD("TAU", Token::CONST_TAU)

#define MIN_KEYWORD_LENGTH 2
#define MAX_KEYWORD_LENGTH 10

#ifdef DEBUG_ENABLED
void GDScriptTokenizerText::make_keyword_list() {
#define KEYWORD_LINE(keyword, token_type) keyword,
#define KEYWORD_GROUP_IGNORE(group)
	keyword_list = {
		KEYWORDS(KEYWORD_GROUP_IGNORE, KEYWORD_LINE)
	};
#undef KEYWORD_LINE
#undef KEYWORD_GROUP_IGNORE
}
#endif // DEBUG_ENABLED

GDScriptTokenizer::Token GDScriptTokenizerText::potential_identifier() {
	bool only_ascii = _peek(-1) < 128;

	// Consume all identifier characters.
	while (is_unicode_identifier_continue(_peek())) {
		char32_t c = _advance();
		only_ascii = only_ascii && c < 128;
	}

	int len = _current - _start;

	if (len == 1 && _peek(-1) == '_') {
		// Lone underscore.
		Token token = make_token(Token::UNDERSCORE);
		token.literal = "_";
		return token;
	}

	String name(_start, len);
	if (len < MIN_KEYWORD_LENGTH || len > MAX_KEYWORD_LENGTH) {
		// Cannot be a keyword, as the length doesn't match any.
		return make_identifier(name);
	}

	if (!only_ascii) {
		// Kept here in case the order with push_error matters.
		Token id = make_identifier(name);

#ifdef DEBUG_ENABLED
		// Additional checks for identifiers but only in debug and if it's available in TextServer.
		if (TS->has_feature(TextServer::FEATURE_UNICODE_SECURITY)) {
			int64_t confusable = TS->is_confusable(name, keyword_list);
			if (confusable >= 0) {
				push_error(vformat(R"(Identifier "%s" is visually similar to the GDScript keyword "%s" and thus not allowed.)", name, keyword_list[confusable]));
			}
		}
#endif // DEBUG_ENABLED

		// Cannot be a keyword, as keywords are ASCII only.
		return id;
	}

	// Define some helper macros for the switch case.
#define KEYWORD_GROUP_CASE(char) \
	break;                       \
	case char:
#define KEYWORD(keyword, token_type)                                                                                      \
	{                                                                                                                     \
		const int keyword_length = sizeof(keyword) - 1;                                                                   \
		static_assert(keyword_length <= MAX_KEYWORD_LENGTH, "There's a keyword longer than the defined maximum length");  \
		static_assert(keyword_length >= MIN_KEYWORD_LENGTH, "There's a keyword shorter than the defined minimum length"); \
		if (keyword_length == len && name == keyword) {                                                                   \
			Token kw = make_token(token_type);                                                                            \
			kw.literal = name;                                                                                            \
			return kw;                                                                                                    \
		}                                                                                                                 \
	}

	// Find if it's a keyword.
	switch (_start[0]) {
		default:
			KEYWORDS(KEYWORD_GROUP_CASE, KEYWORD)
			break;
	}

	// Check if it's a special literal
	if (len == 4) {
		if (name == "true") {
			return make_literal(true);
		} else if (name == "null") {
			return make_literal(Variant());
		}
	} else if (len == 5) {
		if (name == "false") {
			return make_literal(false);
		}
	}

	// Not a keyword, so must be an identifier.
	return make_identifier(name);

#undef KEYWORD_GROUP_CASE
#undef KEYWORD
}

#undef MAX_KEYWORD_LENGTH
#undef MIN_KEYWORD_LENGTH
#undef KEYWORDS

void GDScriptTokenizerText::newline(bool p_make_token) {
	// Don't overwrite previous newline, nor create if we want a line continuation.
	if (p_make_token && !pending_newline && !line_continuation) {
		Token newline(Token::NEWLINE);
		newline.start_line = line;
		newline.end_line = line;
		newline.start_column = column - 1;
		newline.end_column = column;
		newline.leftmost_column = newline.start_column;
		newline.rightmost_column = newline.end_column;
		pending_newline = true;
		last_token = newline;
		last_newline = newline;
	}

	// Increment line/column counters.
	line++;
	column = 1;
	leftmost_column = 1;
}

GDScriptTokenizer::Token GDScriptTokenizerText::number() {
	int base = 10;
	bool has_decimal = false;
	bool has_exponent = false;
	bool has_error = false;
	bool need_digits = false;
	bool (*digit_check_func)(char32_t) = is_digit;

	// Sign before hexadecimal or binary.
	if ((_peek(-1) == '+' || _peek(-1) == '-') && _peek() == '0') {
		_advance();
	}

	if (_peek(-1) == '.') {
		has_decimal = true;
	} else if (_peek(-1) == '0') {
		if (_peek() == 'x') {
			// Hexadecimal.
			base = 16;
			digit_check_func = is_hex_digit;
			need_digits = true;
			_advance();
		} else if (_peek() == 'b') {
			// Binary.
			base = 2;
			digit_check_func = is_binary_digit;
			need_digits = true;
			_advance();
		}
	}

	if (base != 10 && is_underscore(_peek())) { // Disallow `0x_` and `0b_`.
		Token error = make_error(vformat(R"(Unexpected underscore after "0%c".)", _peek(-1)));
		error.start_column = column;
		error.leftmost_column = column;
		error.end_column = column + 1;
		error.rightmost_column = column + 1;
		push_error(error);
		has_error = true;
	}
	bool previous_was_underscore = false; // Allow `_` to be used in a number, for readability.
	while (digit_check_func(_peek()) || is_underscore(_peek())) {
		if (is_underscore(_peek())) {
			if (previous_was_underscore) {
				Token error = make_error(R"(Multiple underscores cannot be adjacent in a numeric literal.)");
				error.start_column = column;
				error.leftmost_column = column;
				error.end_column = column + 1;
				error.rightmost_column = column + 1;
				push_error(error);
			}
			previous_was_underscore = true;
		} else {
			need_digits = false;
			previous_was_underscore = false;
		}
		_advance();
	}

	// It might be a ".." token (instead of decimal point) so we check if it's not.
	if (_peek() == '.' && _peek(1) != '.') {
		if (base == 10 && !has_decimal) {
			has_decimal = true;
		} else if (base == 10) {
			Token error = make_error("Cannot use a decimal point twice in a number.");
			error.start_column = column;
			error.leftmost_column = column;
			error.end_column = column + 1;
			error.rightmost_column = column + 1;
			push_error(error);
			has_error = true;
		} else if (base == 16) {
			Token error = make_error("Cannot use a decimal point in a hexadecimal number.");
			error.start_column = column;
			error.leftmost_column = column;
			error.end_column = column + 1;
			error.rightmost_column = column + 1;
			push_error(error);
			has_error = true;
		} else {
			Token error = make_error("Cannot use a decimal point in a binary number.");
			error.start_column = column;
			error.leftmost_column = column;
			error.end_column = column + 1;
			error.rightmost_column = column + 1;
			push_error(error);
			has_error = true;
		}
		if (!has_error) {
			_advance();

			// Consume decimal digits.
			if (is_underscore(_peek())) { // Disallow `10._`, but allow `10.`.
				Token error = make_error(R"(Unexpected underscore after decimal point.)");
				error.start_column = column;
				error.leftmost_column = column;
				error.end_column = column + 1;
				error.rightmost_column = column + 1;
				push_error(error);
				has_error = true;
			}
			previous_was_underscore = false;
			while (is_digit(_peek()) || is_underscore(_peek())) {
				if (is_underscore(_peek())) {
					if (previous_was_underscore) {
						Token error = make_error(R"(Multiple underscores cannot be adjacent in a numeric literal.)");
						error.start_column = column;
						error.leftmost_column = column;
						error.end_column = column + 1;
						error.rightmost_column = column + 1;
						push_error(error);
					}
					previous_was_underscore = true;
				} else {
					previous_was_underscore = false;
				}
				_advance();
			}
		}
	}
	if (base == 10) {
		if (_peek() == 'e' || _peek() == 'E') {
			has_exponent = true;
			_advance();
			if (_peek() == '+' || _peek() == '-') {
				// Exponent sign.
				_advance();
			}
			// Consume exponent digits.
			if (!is_digit(_peek())) {
				Token error = make_error(R"(Expected exponent value after "e".)");
				error.start_column = column;
				error.leftmost_column = column;
				error.end_column = column + 1;
				error.rightmost_column = column + 1;
				push_error(error);
			}
			previous_was_underscore = false;
			while (is_digit(_peek()) || is_underscore(_peek())) {
				if (is_underscore(_peek())) {
					if (previous_was_underscore) {
						Token error = make_error(R"(Multiple underscores cannot be adjacent in a numeric literal.)");
						error.start_column = column;
						error.leftmost_column = column;
						error.end_column = column + 1;
						error.rightmost_column = column + 1;
						push_error(error);
					}
					previous_was_underscore = true;
				} else {
					previous_was_underscore = false;
				}
				_advance();
			}
		}
	}

	if (need_digits) {
		// No digits in hex or bin literal.
		Token error = make_error(vformat(R"(Expected %s digit after "0%c".)", (base == 16 ? "hexadecimal" : "binary"), (base == 16 ? 'x' : 'b')));
		error.start_column = column;
		error.leftmost_column = column;
		error.end_column = column + 1;
		error.rightmost_column = column + 1;
		return error;
	}

	// Detect extra decimal point.
	if (!has_error && has_decimal && _peek() == '.' && _peek(1) != '.') {
		Token error = make_error("Cannot use a decimal point twice in a number.");
		error.start_column = column;
		error.leftmost_column = column;
		error.end_column = column + 1;
		error.rightmost_column = column + 1;
		push_error(error);
		has_error = true;
	} else if (is_unicode_identifier_start(_peek()) || is_unicode_identifier_continue(_peek())) {
		// Letter at the end of the number.
		push_error("Invalid numeric notation.");
	}

	// Create a string with the whole number.
	int len = _current - _start;
	String number = String(_start, len).replace("_", "");

	// Convert to the appropriate literal type.
	if (base == 16) {
		int64_t value = number.hex_to_int();
		return make_literal(value);
	} else if (base == 2) {
		int64_t value = number.bin_to_int();
		return make_literal(value);
	} else if (has_decimal || has_exponent) {
		double value = number.to_float();
		return make_literal(value);
	} else {
		int64_t value = number.to_int();
		return make_literal(value);
	}
}

GDScriptTokenizer::Token GDScriptTokenizerText::string() {
	enum StringType {
		STRING_REGULAR,
		STRING_NAME,
		STRING_NODEPATH,
	};

	bool is_raw = false;
	bool is_multiline = false;
	StringType type = STRING_REGULAR;

	if (_peek(-1) == 'r') {
		is_raw = true;
		_advance();
	} else if (_peek(-1) == '&') {
		type = STRING_NAME;
		_advance();
	} else if (_peek(-1) == '^') {
		type = STRING_NODEPATH;
		_advance();
	}

	char32_t quote_char = _peek(-1);

	if (_peek() == quote_char && _peek(1) == quote_char) {
		is_multiline = true;
		// Consume all quotes.
		_advance();
		_advance();
	}

	String result;
	char32_t prev = 0;
	int prev_pos = 0;

	for (;;) {
		// Consume actual string.
		if (_is_at_end()) {
			return make_error("Unterminated string.");
		}

		char32_t ch = _peek();

		if (ch == 0x200E || ch == 0x200F || (ch >= 0x202A && ch <= 0x202E) || (ch >= 0x2066 && ch <= 0x2069)) {
			Token error;
			if (is_raw) {
				error = make_error("Invisible text direction control character present in the string, use regular string literal instead of r-string.");
			} else {
				error = make_error("Invisible text direction control character present in the string, escape it (\"\\u" + String::num_int64(ch, 16) + "\") to avoid confusion.");
			}
			error.start_column = column;
			error.leftmost_column = error.start_column;
			error.end_column = column + 1;
			error.rightmost_column = error.end_column;
			push_error(error);
		}

		if (ch == '\\') {
			// Escape pattern.
			_advance();
			if (_is_at_end()) {
				return make_error("Unterminated string.");
			}

			if (is_raw) {
				if (_peek() == quote_char) {
					_advance();
					if (_is_at_end()) {
						return make_error("Unterminated string.");
					}
					result += '\\';
					result += quote_char;
				} else if (_peek() == '\\') { // For `\\\"`.
					_advance();
					if (_is_at_end()) {
						return make_error("Unterminated string.");
					}
					result += '\\';
					result += '\\';
				} else {
					result += '\\';
				}
			} else {
				// Grab escape character.
				char32_t code = _peek();
				_advance();
				if (_is_at_end()) {
					return make_error("Unterminated string.");
				}

				char32_t escaped = 0;
				bool valid_escape = true;

				switch (code) {
					case 'a':
						escaped = '\a';
						break;
					case 'b':
						escaped = '\b';
						break;
					case 'f':
						escaped = '\f';
						break;
					case 'n':
						escaped = '\n';
						break;
					case 'r':
						escaped = '\r';
						break;
					case 't':
						escaped = '\t';
						break;
					case 'v':
						escaped = '\v';
						break;
					case '\'':
						escaped = '\'';
						break;
					case '\"':
						escaped = '\"';
						break;
					case '\\':
						escaped = '\\';
						break;
					case 'U':
					case 'u': {
						// Hexadecimal sequence.
						int hex_len = (code == 'U') ? 6 : 4;
						for (int j = 0; j < hex_len; j++) {
							if (_is_at_end()) {
								return make_error("Unterminated string.");
							}

							char32_t digit = _peek();
							char32_t value = 0;
							if (is_digit(digit)) {
								value = digit - '0';
							} else if (digit >= 'a' && digit <= 'f') {
								value = digit - 'a';
								value += 10;
							} else if (digit >= 'A' && digit <= 'F') {
								value = digit - 'A';
								value += 10;
							} else {
								// Make error, but keep parsing the string.
								Token error = make_error("Invalid hexadecimal digit in unicode escape sequence.");
								error.start_column = column;
								error.leftmost_column = error.start_column;
								error.end_column = column + 1;
								error.rightmost_column = error.end_column;
								push_error(error);
								valid_escape = false;
								break;
							}

							escaped <<= 4;
							escaped |= value;

							_advance();
						}
					} break;
					case '\r':
						if (_peek() != '\n') {
							// Carriage return without newline in string. (???)
							// Just add it to the string and keep going.
							result += ch;
							_advance();
							break;
						}
						[[fallthrough]];
					case '\n':
						// Escaping newline.
						newline(false);
						valid_escape = false; // Don't add to the string.
						break;
					default:
						Token error = make_error("Invalid escape in string.");
						error.start_column = column - 2;
						error.leftmost_column = error.start_column;
						push_error(error);
						valid_escape = false;
						break;
				}
				// Parse UTF-16 pair.
				if (valid_escape) {
					if ((escaped & 0xfffffc00) == 0xd800) {
						if (prev == 0) {
							prev = escaped;
							prev_pos = column - 2;
							continue;
						} else {
							Token error = make_error("Invalid UTF-16 sequence in string, unpaired lead surrogate.");
							error.start_column = column - 2;
							error.leftmost_column = error.start_column;
							push_error(error);
							valid_escape = false;
							prev = 0;
						}
					} else if ((escaped & 0xfffffc00) == 0xdc00) {
						if (prev == 0) {
							Token error = make_error("Invalid UTF-16 sequence in string, unpaired trail surrogate.");
							error.start_column = column - 2;
							error.leftmost_column = error.start_column;
							push_error(error);
							valid_escape = false;
						} else {
							escaped = (prev << 10UL) + escaped - ((0xd800 << 10UL) + 0xdc00 - 0x10000);
							prev = 0;
						}
					}
					if (prev != 0) {
						Token error = make_error("Invalid UTF-16 sequence in string, unpaired lead surrogate.");
						error.start_column = prev_pos;
						error.leftmost_column = error.start_column;
						push_error(error);
						prev = 0;
					}
				}

				if (valid_escape) {
					result += escaped;
				}
			}
		} else if (ch == quote_char) {
			if (prev != 0) {
				Token error = make_error("Invalid UTF-16 sequence in string, unpaired lead surrogate");
				error.start_column = prev_pos;
				error.leftmost_column = error.start_column;
				push_error(error);
				prev = 0;
			}
			_advance();
			if (is_multiline) {
				if (_peek() == quote_char && _peek(1) == quote_char) {
					// Ended the multiline string. Consume all quotes.
					_advance();
					_advance();
					break;
				} else {
					// Not a multiline string termination, add consumed quote.
					result += quote_char;
				}
			} else {
				// Ended single-line string.
				break;
			}
		} else {
			if (prev != 0) {
				Token error = make_error("Invalid UTF-16 sequence in string, unpaired lead surrogate");
				error.start_column = prev_pos;
				error.leftmost_column = error.start_column;
				push_error(error);
				prev = 0;
			}
			result += ch;
			_advance();
			if (ch == '\n') {
				newline(false);
			}
		}
	}
	if (prev != 0) {
		Token error = make_error("Invalid UTF-16 sequence in string, unpaired lead surrogate");
		error.start_column = prev_pos;
		error.leftmost_column = error.start_column;
		push_error(error);
		prev = 0;
	}

	// Make the literal.
	Variant string;
	switch (type) {
		case STRING_NAME:
			string = StringName(result);
			break;
		case STRING_NODEPATH:
			string = NodePath(result);
			break;
		case STRING_REGULAR:
			string = result;
			break;
	}

	return make_literal(string);
}

void GDScriptTokenizerText::check_indent() {
	ERR_FAIL_COND_MSG(column != 1, "Checking tokenizer indentation in the middle of a line.");

	if (_is_at_end()) {
		// Send dedents for every indent level.
		pending_indents -= indent_level();
		indent_stack.clear();
		return;
	}

	for (;;) {
		char32_t current_indent_char = _peek();
		int indent_count = 0;

		if (current_indent_char != ' ' && current_indent_char != '\t' && current_indent_char != '\r' && current_indent_char != '\n' && current_indent_char != '#') {
			// First character of the line is not whitespace, so we clear all indentation levels.
			// Unless we are in a continuation or in multiline mode (inside expression).
			if (line_continuation || multiline_mode) {
				return;
			}
			pending_indents -= indent_level();
			indent_stack.clear();
			return;
		}

		if (_peek() == '\r') {
			_advance();
			if (_peek() != '\n') {
				push_error("Stray carriage return character in source code.");
			}
		}
		if (_peek() == '\n') {
			// Empty line, keep going.
			_advance();
			newline(false);
			continue;
		}

		// Check indent level.
		bool mixed = false;
		while (!_is_at_end()) {
			char32_t space = _peek();
			if (space == '\t') {
				// Consider individual tab columns.
				column += tab_size - 1;
				indent_count += tab_size;
			} else if (space == ' ') {
				indent_count += 1;
			} else {
				break;
			}
			mixed = mixed || space != current_indent_char;
			_advance();
		}

		if (_is_at_end()) {
			// Reached the end with an empty line, so just dedent as much as needed.
			pending_indents -= indent_level();
			indent_stack.clear();
			return;
		}

		if (_peek() == '\r') {
			_advance();
			if (_peek() != '\n') {
				push_error("Stray carriage return character in source code.");
			}
		}
		if (_peek() == '\n') {
			// Empty line, keep going.
			_advance();
			newline(false);
			continue;
		}
		if (_peek() == '#') {
			// Comment. Advance to the next line.
#ifdef TOOLS_ENABLED
			String comment;
			while (_peek() != '\n' && !_is_at_end()) {
				comment += _advance();
			}
			comments[line] = CommentData(comment, true);
#else
			while (_peek() != '\n' && !_is_at_end()) {
				_advance();
			}
#endif // TOOLS_ENABLED
			if (_is_at_end()) {
				// Reached the end with an empty line, so just dedent as much as needed.
				pending_indents -= indent_level();
				indent_stack.clear();
				return;
			}
			_advance(); // Consume '\n'.
			newline(false);
			continue;
		}

		if (mixed && !line_continuation && !multiline_mode) {
			Token error = make_error("Mixed use of tabs and spaces for indentation.");
			error.start_line = line;
			error.start_column = 1;
			error.leftmost_column = 1;
			error.rightmost_column = column;
			push_error(error);
		}

		if (line_continuation || multiline_mode) {
			// We cleared up all the whitespace at the beginning of the line.
			// If this is a line continuation or we're in multiline mode then we don't want any indentation changes.
			return;
		}

		// Check if indentation character is consistent.
		if (indent_char == '\0') {
			// First time indenting, choose character now.
			indent_char = current_indent_char;
		} else if (current_indent_char != indent_char) {
			Token error = make_error(vformat("Used %s character for indentation instead of %s as used before in the file.",
					_get_indent_char_name(current_indent_char), _get_indent_char_name(indent_char)));
			error.start_line = line;
			error.start_column = 1;
			error.leftmost_column = 1;
			error.rightmost_column = column;
			push_error(error);
		}

		// Now we can do actual indentation changes.

		// Check if indent or dedent.
		int previous_indent = 0;
		if (indent_level() > 0) {
			previous_indent = indent_stack.back()->get();
		}
		if (indent_count == previous_indent) {
			// No change in indentation.
			return;
		}
		if (indent_count > previous_indent) {
			// Indentation increased.
			indent_stack.push_back(indent_count);
			pending_indents++;
		} else {
			// Indentation decreased (dedent).
			if (indent_level() == 0) {
				push_error("Tokenizer bug: trying to dedent without previous indent.");
				return;
			}
			while (indent_level() > 0 && indent_stack.back()->get() > indent_count) {
				indent_stack.pop_back();
				pending_indents--;
			}
			if ((indent_level() > 0 && indent_stack.back()->get() != indent_count) || (indent_level() == 0 && indent_count != 0)) {
				// Mismatched indentation alignment.
				Token error = make_error("Unindent doesn't match the previous indentation level.");
				error.start_line = line;
				error.start_column = 1;
				error.leftmost_column = 1;
				error.end_column = column + 1;
				error.rightmost_column = column + 1;
				push_error(error);
				// Still, we'll be lenient and keep going, so keep this level in the stack.
				indent_stack.push_back(indent_count);
			}
		}
		break; // Get out of the loop in any case.
	}
}

String GDScriptTokenizerText::_get_indent_char_name(char32_t ch) {
	ERR_FAIL_COND_V(ch != ' ' && ch != '\t', String(&ch, 1).c_escape());

	return ch == ' ' ? "space" : "tab";
}

void GDScriptTokenizerText::_skip_whitespace() {
	if (pending_indents != 0) {
		// Still have some indent/dedent tokens to give.
		return;
	}

	bool is_bol = column == 1; // Beginning of line.

	if (is_bol) {
		check_indent();
		return;
	}

	for (;;) {
		char32_t c = _peek();
		switch (c) {
			case ' ':
				_advance();
				break;
			case '\t':
				_advance();
				// Consider individual tab columns.
				column += tab_size - 1;
				break;
			case '\r':
				_advance(); // Consume either way.
				if (_peek() != '\n') {
					push_error("Stray carriage return character in source code.");
					return;
				}
				break;
			case '\n':
				_advance();
				newline(!is_bol); // Don't create new line token if line is empty.
				check_indent();
				break;
			case '#': {
				// Comment.
#ifdef TOOLS_ENABLED
				String comment;
				while (_peek() != '\n' && !_is_at_end()) {
					comment += _advance();
				}
				comments[line] = CommentData(comment, is_bol);
#else
				while (_peek() != '\n' && !_is_at_end()) {
					_advance();
				}
#endif // TOOLS_ENABLED
				if (_is_at_end()) {
					return;
				}
				_advance(); // Consume '\n'
				newline(!is_bol);
				check_indent();
			} break;
			default:
				return;
		}
	}
}

GDScriptTokenizer::Token GDScriptTokenizerText::scan() {
	if (has_error()) {
		return pop_error();
	}

	_skip_whitespace();

	if (pending_newline) {
		pending_newline = false;
		if (!multiline_mode) {
			// Don't return newline tokens on multiline mode.
			return last_newline;
		}
	}

	// Check for potential errors after skipping whitespace().
	if (has_error()) {
		return pop_error();
	}

	_start = _current;
	start_line = line;
	start_column = column;
	leftmost_column = column;
	rightmost_column = column;

	if (pending_indents != 0) {
		// Adjust position for indent.
		_start -= start_column - 1;
		start_column = 1;
		leftmost_column = 1;
		if (pending_indents > 0) {
			// Indents.
			pending_indents--;
			return make_token(Token::INDENT);
		} else {
			// Dedents.
			pending_indents++;
			Token dedent = make_token(Token::DEDENT);
			dedent.end_column += 1;
			dedent.rightmost_column += 1;
			return dedent;
		}
	}

	if (_is_at_end()) {
		return make_token(Token::TK_EOF);
	}

	const char32_t c = _advance();

	if (c == '\\') {
		// Line continuation with backslash.
		if (_peek() == '\r') {
			if (_peek(1) != '\n') {
				return make_error("Unexpected carriage return character.");
			}
			_advance();
		}
		if (_peek() != '\n') {
			return make_error("Expected new line after \"\\\".");
		}
		_advance();
		newline(false);
		line_continuation = true;
		_skip_whitespace(); // Skip whitespace/comment lines after `\`. See GH-89403.
		continuation_lines.push_back(line);
		return scan(); // Recurse to get next token.
	}

	line_continuation = false;

	if (is_digit(c)) {
		return number();
	} else if (c == 'r' && (_peek() == '"' || _peek() == '\'')) {
		// Raw string literals.
		return string();
	} else if (is_unicode_identifier_start(c)) {
		return potential_identifier();
	}

	switch (c) {
		// String literals.
		case '"':
		case '\'':
			return string();

		// Annotation.
		case '@':
			return annotation();

		// Single characters.
		case '~':
			return make_token(Token::TILDE);
		case ',':
			return make_token(Token::COMMA);
		case ':':
			return make_token(Token::COLON);
		case ';':
			return make_token(Token::SEMICOLON);
		case '$':
			return make_token(Token::DOLLAR);
		case '?':
			return make_token(Token::QUESTION_MARK);
		case '`':
			return make_token(Token::BACKTICK);

		// Parens.
		case '(':
			push_paren('(');
			return make_token(Token::PARENTHESIS_OPEN);
		case '[':
			push_paren('[');
			return make_token(Token::BRACKET_OPEN);
		case '{':
			push_paren('{');
			return make_token(Token::BRACE_OPEN);
		case ')':
			if (!pop_paren('(')) {
				return make_paren_error(c);
			}
			return make_token(Token::PARENTHESIS_CLOSE);
		case ']':
			if (!pop_paren('[')) {
				return make_paren_error(c);
			}
			return make_token(Token::BRACKET_CLOSE);
		case '}':
			if (!pop_paren('{')) {
				return make_paren_error(c);
			}
			return make_token(Token::BRACE_CLOSE);

		// Double characters.
		case '!':
			if (_peek() == '=') {
				_advance();
				return make_token(Token::BANG_EQUAL);
			} else {
				return make_token(Token::BANG);
			}
		case '.':
			if (_peek() == '.') {
				_advance();
				return make_token(Token::PERIOD_PERIOD);
			} else if (is_digit(_peek())) {
				// Number starting with '.'.
				return number();
			} else {
				return make_token(Token::PERIOD);
			}
		case '+':
			if (_peek() == '=') {
				_advance();
				return make_token(Token::PLUS_EQUAL);
			} else if (is_digit(_peek()) && !last_token.can_precede_bin_op()) {
				// Number starting with '+'.
				return number();
			} else {
				return make_token(Token::PLUS);
			}
		case '-':
			if (_peek() == '=') {
				_advance();
				return make_token(Token::MINUS_EQUAL);
			} else if (is_digit(_peek()) && !last_token.can_precede_bin_op()) {
				// Number starting with '-'.
				return number();
			} else if (_peek() == '>') {
				_advance();
				return make_token(Token::FORWARD_ARROW);
			} else {
				return make_token(Token::MINUS);
			}
		case '*':
			if (_peek() == '=') {
				_advance();
				return make_token(Token::STAR_EQUAL);
			} else if (_peek() == '*') {
				if (_peek(1) == '=') {
					_advance();
					_advance(); // Advance both '*' and '='
					return make_token(Token::STAR_STAR_EQUAL);
				}
				_advance();
				return make_token(Token::STAR_STAR);
			} else {
				return make_token(Token::STAR);
			}
		case '/':
			if (_peek() == '=') {
				_advance();
				return make_token(Token::SLASH_EQUAL);
			} else {
				return make_token(Token::SLASH);
			}
		case '%':
			if (_peek() == '=') {
				_advance();
				return make_token(Token::PERCENT_EQUAL);
			} else {
				return make_token(Token::PERCENT);
			}
		case '^':
			if (_peek() == '=') {
				_advance();
				return make_token(Token::CARET_EQUAL);
			} else if (_peek() == '"' || _peek() == '\'') {
				// Node path
				return string();
			} else {
				return make_token(Token::CARET);
			}
		case '&':
			if (_peek() == '&') {
				_advance();
				return make_token(Token::AMPERSAND_AMPERSAND);
			} else if (_peek() == '=') {
				_advance();
				return make_token(Token::AMPERSAND_EQUAL);
			} else if (_peek() == '"' || _peek() == '\'') {
				// String Name
				return string();
			} else {
				return make_token(Token::AMPERSAND);
			}
		case '|':
			if (_peek() == '|') {
				_advance();
				return make_token(Token::PIPE_PIPE);
			} else if (_peek() == '=') {
				_advance();
				return make_token(Token::PIPE_EQUAL);
			} else {
				return make_token(Token::PIPE);
			}

		// Potential VCS conflict markers.
		case '=':
			if (_peek() == '=') {
				return check_vcs_marker('=', Token::EQUAL_EQUAL);
			} else {
				return make_token(Token::EQUAL);
			}
		case '<':
			if (_peek() == '=') {
				_advance();
				return make_token(Token::LESS_EQUAL);
			} else if (_peek() == '<') {
				if (_peek(1) == '=') {
					_advance();
					_advance(); // Advance both '<' and '='
					return make_token(Token::LESS_LESS_EQUAL);
				} else {
					return check_vcs_marker('<', Token::LESS_LESS);
				}
			} else {
				return make_token(Token::LESS);
			}
		case '>':
			if (_peek() == '=') {
				_advance();
				return make_token(Token::GREATER_EQUAL);
			} else if (_peek() == '>') {
				if (_peek(1) == '=') {
					_advance();
					_advance(); // Advance both '>' and '='
					return make_token(Token::GREATER_GREATER_EQUAL);
				} else {
					return check_vcs_marker('>', Token::GREATER_GREATER);
				}
			} else {
				return make_token(Token::GREATER);
			}

		default:
			if (is_whitespace(c)) {
				return make_error(vformat(R"(Invalid white space character U+%04X.)", static_cast<int32_t>(c)));
			} else {
				return make_error(vformat(R"(Invalid character "%c" (U+%04X).)", c, static_cast<int32_t>(c)));
			}
	}
}

GDScriptTokenizerText::GDScriptTokenizerText() {
#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton()) {
		tab_size = EditorSettings::get_singleton()->get_setting("text_editor/behavior/indent/size");
	}
#endif // TOOLS_ENABLED
#ifdef DEBUG_ENABLED
	make_keyword_list();
#endif // DEBUG_ENABLED
}
