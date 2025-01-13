/**************************************************************************/
/*  expression.cpp                                                        */
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

#include "expression.h"

#include "core/object/class_db.h"

Error Expression::_get_token(Token &r_token) {
	while (true) {
#define GET_CHAR() (str_ofs >= expression.length() ? 0 : expression[str_ofs++])

		char32_t cchar = GET_CHAR();

		switch (cchar) {
			case 0: {
				r_token.type = TK_EOF;
				return OK;
			}
			case '{': {
				r_token.type = TK_CURLY_BRACKET_OPEN;
				return OK;
			}
			case '}': {
				r_token.type = TK_CURLY_BRACKET_CLOSE;
				return OK;
			}
			case '[': {
				r_token.type = TK_BRACKET_OPEN;
				return OK;
			}
			case ']': {
				r_token.type = TK_BRACKET_CLOSE;
				return OK;
			}
			case '(': {
				r_token.type = TK_PARENTHESIS_OPEN;
				return OK;
			}
			case ')': {
				r_token.type = TK_PARENTHESIS_CLOSE;
				return OK;
			}
			case ',': {
				r_token.type = TK_COMMA;
				return OK;
			}
			case ':': {
				r_token.type = TK_COLON;
				return OK;
			}
			case '$': {
				r_token.type = TK_INPUT;
				int index = 0;
				do {
					if (!is_digit(expression[str_ofs])) {
						_set_error("Expected number after '$'");
						r_token.type = TK_ERROR;
						return ERR_PARSE_ERROR;
					}
					index *= 10;
					index += expression[str_ofs] - '0';
					str_ofs++;

				} while (is_digit(expression[str_ofs]));

				r_token.value = index;
				return OK;
			}
			case '=': {
				cchar = GET_CHAR();
				if (cchar == '=') {
					r_token.type = TK_OP_EQUAL;
				} else {
					_set_error("Expected '='");
					r_token.type = TK_ERROR;
					return ERR_PARSE_ERROR;
				}
				return OK;
			}
			case '!': {
				if (expression[str_ofs] == '=') {
					r_token.type = TK_OP_NOT_EQUAL;
					str_ofs++;
				} else {
					r_token.type = TK_OP_NOT;
				}
				return OK;
			}
			case '>': {
				if (expression[str_ofs] == '=') {
					r_token.type = TK_OP_GREATER_EQUAL;
					str_ofs++;
				} else if (expression[str_ofs] == '>') {
					r_token.type = TK_OP_SHIFT_RIGHT;
					str_ofs++;
				} else {
					r_token.type = TK_OP_GREATER;
				}
				return OK;
			}
			case '<': {
				if (expression[str_ofs] == '=') {
					r_token.type = TK_OP_LESS_EQUAL;
					str_ofs++;
				} else if (expression[str_ofs] == '<') {
					r_token.type = TK_OP_SHIFT_LEFT;
					str_ofs++;
				} else {
					r_token.type = TK_OP_LESS;
				}
				return OK;
			}
			case '+': {
				r_token.type = TK_OP_ADD;
				return OK;
			}
			case '-': {
				r_token.type = TK_OP_SUB;
				return OK;
			}
			case '/': {
				r_token.type = TK_OP_DIV;
				return OK;
			}
			case '*': {
				if (expression[str_ofs] == '*') {
					r_token.type = TK_OP_POW;
					str_ofs++;
				} else {
					r_token.type = TK_OP_MUL;
				}
				return OK;
			}
			case '%': {
				r_token.type = TK_OP_MOD;
				return OK;
			}
			case '&': {
				if (expression[str_ofs] == '&') {
					r_token.type = TK_OP_AND;
					str_ofs++;
				} else {
					r_token.type = TK_OP_BIT_AND;
				}
				return OK;
			}
			case '|': {
				if (expression[str_ofs] == '|') {
					r_token.type = TK_OP_OR;
					str_ofs++;
				} else {
					r_token.type = TK_OP_BIT_OR;
				}
				return OK;
			}
			case '^': {
				r_token.type = TK_OP_BIT_XOR;

				return OK;
			}
			case '~': {
				r_token.type = TK_OP_BIT_INVERT;

				return OK;
			}
			case '\'':
			case '"': {
				String str;
				char32_t prev = 0;
				while (true) {
					char32_t ch = GET_CHAR();

					if (ch == 0) {
						_set_error("Unterminated String");
						r_token.type = TK_ERROR;
						return ERR_PARSE_ERROR;
					} else if (ch == cchar) {
						// cchar contain a corresponding quote symbol
						break;
					} else if (ch == '\\') {
						//escaped characters...

						char32_t next = GET_CHAR();
						if (next == 0) {
							_set_error("Unterminated String");
							r_token.type = TK_ERROR;
							return ERR_PARSE_ERROR;
						}
						char32_t res = 0;

						switch (next) {
							case 'b':
								res = 8;
								break;
							case 't':
								res = 9;
								break;
							case 'n':
								res = 10;
								break;
							case 'f':
								res = 12;
								break;
							case 'r':
								res = 13;
								break;
							case 'U':
							case 'u': {
								// Hexadecimal sequence.
								int hex_len = (next == 'U') ? 6 : 4;
								for (int j = 0; j < hex_len; j++) {
									char32_t c = GET_CHAR();

									if (c == 0) {
										_set_error("Unterminated String");
										r_token.type = TK_ERROR;
										return ERR_PARSE_ERROR;
									}
									if (!is_hex_digit(c)) {
										_set_error("Malformed hex constant in string");
										r_token.type = TK_ERROR;
										return ERR_PARSE_ERROR;
									}
									char32_t v;
									if (is_digit(c)) {
										v = c - '0';
									} else if (c >= 'a' && c <= 'f') {
										v = c - 'a';
										v += 10;
									} else if (c >= 'A' && c <= 'F') {
										v = c - 'A';
										v += 10;
									} else {
										ERR_PRINT("Bug parsing hex constant.");
										v = 0;
									}

									res <<= 4;
									res |= v;
								}

							} break;
							default: {
								res = next;
							} break;
						}

						// Parse UTF-16 pair.
						if ((res & 0xfffffc00) == 0xd800) {
							if (prev == 0) {
								prev = res;
								continue;
							} else {
								_set_error("Invalid UTF-16 sequence in string, unpaired lead surrogate");
								r_token.type = TK_ERROR;
								return ERR_PARSE_ERROR;
							}
						} else if ((res & 0xfffffc00) == 0xdc00) {
							if (prev == 0) {
								_set_error("Invalid UTF-16 sequence in string, unpaired trail surrogate");
								r_token.type = TK_ERROR;
								return ERR_PARSE_ERROR;
							} else {
								res = (prev << 10UL) + res - ((0xd800 << 10UL) + 0xdc00 - 0x10000);
								prev = 0;
							}
						}
						if (prev != 0) {
							_set_error("Invalid UTF-16 sequence in string, unpaired lead surrogate");
							r_token.type = TK_ERROR;
							return ERR_PARSE_ERROR;
						}
						str += res;
					} else {
						if (prev != 0) {
							_set_error("Invalid UTF-16 sequence in string, unpaired lead surrogate");
							r_token.type = TK_ERROR;
							return ERR_PARSE_ERROR;
						}
						str += ch;
					}
				}
				if (prev != 0) {
					_set_error("Invalid UTF-16 sequence in string, unpaired lead surrogate");
					r_token.type = TK_ERROR;
					return ERR_PARSE_ERROR;
				}

				r_token.type = TK_CONSTANT;
				r_token.value = str;
				return OK;

			} break;
			default: {
				if (cchar <= 32) {
					break;
				}

				char32_t next_char = (str_ofs >= expression.length()) ? 0 : expression[str_ofs];
				if (is_digit(cchar) || (cchar == '.' && is_digit(next_char))) {
					//a number

					String num;
#define READING_SIGN 0
#define READING_INT 1
#define READING_HEX 2
#define READING_BIN 3
#define READING_DEC 4
#define READING_EXP 5
#define READING_DONE 6
					int reading = READING_INT;

					char32_t c = cchar;
					bool exp_sign = false;
					bool exp_beg = false;
					bool bin_beg = false;
					bool hex_beg = false;
					bool is_float = false;
					bool is_first_char = true;

					while (true) {
						switch (reading) {
							case READING_INT: {
								if (is_digit(c)) {
									if (is_first_char && c == '0') {
										if (next_char == 'b') {
											reading = READING_BIN;
										} else if (next_char == 'x') {
											reading = READING_HEX;
										}
									}
								} else if (c == '.') {
									reading = READING_DEC;
									is_float = true;
								} else if (c == 'e') {
									reading = READING_EXP;
									is_float = true;
								} else {
									reading = READING_DONE;
								}

							} break;
							case READING_BIN: {
								if (bin_beg && !is_binary_digit(c)) {
									reading = READING_DONE;
								} else if (c == 'b') {
									bin_beg = true;
								}

							} break;
							case READING_HEX: {
								if (hex_beg && !is_hex_digit(c)) {
									reading = READING_DONE;
								} else if (c == 'x') {
									hex_beg = true;
								}

							} break;
							case READING_DEC: {
								if (is_digit(c)) {
								} else if (c == 'e') {
									reading = READING_EXP;
								} else {
									reading = READING_DONE;
								}

							} break;
							case READING_EXP: {
								if (is_digit(c)) {
									exp_beg = true;

								} else if ((c == '-' || c == '+') && !exp_sign && !exp_beg) {
									exp_sign = true;

								} else {
									reading = READING_DONE;
								}
							} break;
						}

						if (reading == READING_DONE) {
							break;
						}
						num += String::chr(c);
						c = GET_CHAR();
						is_first_char = false;
					}

					if (c != 0) {
						str_ofs--;
					}

					r_token.type = TK_CONSTANT;

					if (is_float) {
						r_token.value = num.to_float();
					} else if (bin_beg) {
						r_token.value = num.bin_to_int();
					} else if (hex_beg) {
						r_token.value = num.hex_to_int();
					} else {
						r_token.value = num.to_int();
					}
					return OK;

				} else if (is_unicode_identifier_start(cchar)) {
					String id = String::chr(cchar);
					cchar = GET_CHAR();

					while (is_unicode_identifier_continue(cchar)) {
						id += String::chr(cchar);
						cchar = GET_CHAR();
					}

					str_ofs--; //go back one

					if (id == "in") {
						r_token.type = TK_OP_IN;
					} else if (id == "null") {
						r_token.type = TK_CONSTANT;
						r_token.value = Variant();
					} else if (id == "true") {
						r_token.type = TK_CONSTANT;
						r_token.value = true;
					} else if (id == "false") {
						r_token.type = TK_CONSTANT;
						r_token.value = false;
					} else if (id == "PI") {
						r_token.type = TK_CONSTANT;
						r_token.value = Math_PI;
					} else if (id == "TAU") {
						r_token.type = TK_CONSTANT;
						r_token.value = Math_TAU;
					} else if (id == "INF") {
						r_token.type = TK_CONSTANT;
						r_token.value = INFINITY;
					} else if (id == "NAN") {
						r_token.type = TK_CONSTANT;
						r_token.value = NAN;
					} else if (id == "not") {
						r_token.type = TK_OP_NOT;
					} else if (id == "or") {
						r_token.type = TK_OP_OR;
					} else if (id == "and") {
						r_token.type = TK_OP_AND;
					} else if (id == "self") {
						r_token.type = TK_SELF;
					} else {
						{
							const Variant::Type type = Variant::get_type_by_name(id);
							if (type < Variant::VARIANT_MAX) {
								r_token.type = TK_BASIC_TYPE;
								r_token.value = type;
								return OK;
							}
						}

						if (Variant::has_utility_function(id)) {
							r_token.type = TK_BUILTIN_FUNC;
							r_token.value = id;
							return OK;
						}

						r_token.type = TK_IDENTIFIER;
						r_token.value = id;
					}

					return OK;

				} else if (cchar == '.') {
					// Handled down there as we support '.[0-9]' as numbers above
					r_token.type = TK_PERIOD;
					return OK;

				} else {
					_set_error("Unexpected character.");
					r_token.type = TK_ERROR;
					return ERR_PARSE_ERROR;
				}
			}
		}
#undef GET_CHAR
	}

	r_token.type = TK_ERROR;
	return ERR_PARSE_ERROR;
}

const char *Expression::token_name[TK_MAX] = {
	"CURLY BRACKET OPEN",
	"CURLY BRACKET CLOSE",
	"BRACKET OPEN",
	"BRACKET CLOSE",
	"PARENTHESIS OPEN",
	"PARENTHESIS CLOSE",
	"IDENTIFIER",
	"BUILTIN FUNC",
	"SELF",
	"CONSTANT",
	"BASIC TYPE",
	"COLON",
	"COMMA",
	"PERIOD",
	"OP IN",
	"OP EQUAL",
	"OP NOT EQUAL",
	"OP LESS",
	"OP LESS EQUAL",
	"OP GREATER",
	"OP GREATER EQUAL",
	"OP AND",
	"OP OR",
	"OP NOT",
	"OP ADD",
	"OP SUB",
	"OP MUL",
	"OP DIV",
	"OP MOD",
	"OP POW",
	"OP SHIFT LEFT",
	"OP SHIFT RIGHT",
	"OP BIT AND",
	"OP BIT OR",
	"OP BIT XOR",
	"OP BIT INVERT",
	"OP INPUT",
	"EOF",
	"ERROR"
};

Expression::ENode *Expression::_parse_expression() {
	Vector<ExpressionNode> expression_nodes;

	while (true) {
		//keep appending stuff to expression
		ENode *expr = nullptr;

		Token tk;
		_get_token(tk);
		if (error_set) {
			return nullptr;
		}

		switch (tk.type) {
			case TK_CURLY_BRACKET_OPEN: {
				//a dictionary
				DictionaryNode *dn = alloc_node<DictionaryNode>();

				while (true) {
					int cofs = str_ofs;
					_get_token(tk);
					if (tk.type == TK_CURLY_BRACKET_CLOSE) {
						break;
					}
					str_ofs = cofs; //revert
					//parse an expression
					ENode *subexpr = _parse_expression();
					if (!subexpr) {
						return nullptr;
					}
					dn->dict.push_back(subexpr);

					_get_token(tk);
					if (tk.type != TK_COLON) {
						_set_error("Expected ':'");
						return nullptr;
					}

					subexpr = _parse_expression();
					if (!subexpr) {
						return nullptr;
					}

					dn->dict.push_back(subexpr);

					cofs = str_ofs;
					_get_token(tk);
					if (tk.type == TK_COMMA) {
						//all good
					} else if (tk.type == TK_CURLY_BRACKET_CLOSE) {
						str_ofs = cofs;
					} else {
						_set_error("Expected ',' or '}'");
					}
				}

				expr = dn;
			} break;
			case TK_BRACKET_OPEN: {
				//an array

				ArrayNode *an = alloc_node<ArrayNode>();

				while (true) {
					int cofs = str_ofs;
					_get_token(tk);
					if (tk.type == TK_BRACKET_CLOSE) {
						break;
					}
					str_ofs = cofs; //revert
					//parse an expression
					ENode *subexpr = _parse_expression();
					if (!subexpr) {
						return nullptr;
					}
					an->array.push_back(subexpr);

					cofs = str_ofs;
					_get_token(tk);
					if (tk.type == TK_COMMA) {
						//all good
					} else if (tk.type == TK_BRACKET_CLOSE) {
						str_ofs = cofs;
					} else {
						_set_error("Expected ',' or ']'");
					}
				}

				expr = an;
			} break;
			case TK_PARENTHESIS_OPEN: {
				//a suexpression
				ENode *e = _parse_expression();
				if (error_set) {
					return nullptr;
				}
				_get_token(tk);
				if (tk.type != TK_PARENTHESIS_CLOSE) {
					_set_error("Expected ')'");
					return nullptr;
				}

				expr = e;

			} break;
			case TK_IDENTIFIER: {
				String identifier = tk.value;

				int cofs = str_ofs;
				_get_token(tk);
				if (tk.type == TK_PARENTHESIS_OPEN) {
					//function call
					CallNode *func_call = alloc_node<CallNode>();
					func_call->method = identifier;
					SelfNode *self_node = alloc_node<SelfNode>();
					func_call->base = self_node;

					while (true) {
						int cofs2 = str_ofs;
						_get_token(tk);
						if (tk.type == TK_PARENTHESIS_CLOSE) {
							break;
						}
						str_ofs = cofs2; //revert
						//parse an expression
						ENode *subexpr = _parse_expression();
						if (!subexpr) {
							return nullptr;
						}

						func_call->arguments.push_back(subexpr);

						cofs2 = str_ofs;
						_get_token(tk);
						if (tk.type == TK_COMMA) {
							//all good
						} else if (tk.type == TK_PARENTHESIS_CLOSE) {
							str_ofs = cofs2;
						} else {
							_set_error("Expected ',' or ')'");
						}
					}

					expr = func_call;
				} else {
					//named indexing
					str_ofs = cofs;

					int input_index = -1;
					for (int i = 0; i < input_names.size(); i++) {
						if (input_names[i] == identifier) {
							input_index = i;
							break;
						}
					}

					if (input_index != -1) {
						InputNode *input = alloc_node<InputNode>();
						input->index = input_index;
						expr = input;
					} else {
						NamedIndexNode *index = alloc_node<NamedIndexNode>();
						SelfNode *self_node = alloc_node<SelfNode>();
						index->base = self_node;
						index->name = identifier;
						expr = index;
					}
				}
			} break;
			case TK_INPUT: {
				InputNode *input = alloc_node<InputNode>();
				input->index = tk.value;
				expr = input;
			} break;
			case TK_SELF: {
				SelfNode *self = alloc_node<SelfNode>();
				expr = self;
			} break;
			case TK_CONSTANT: {
				ConstantNode *constant = alloc_node<ConstantNode>();
				constant->value = tk.value;
				expr = constant;
			} break;
			case TK_BASIC_TYPE: {
				//constructor..

				Variant::Type bt = Variant::Type(int(tk.value));
				_get_token(tk);
				if (tk.type != TK_PARENTHESIS_OPEN) {
					_set_error("Expected '('");
					return nullptr;
				}

				ConstructorNode *constructor = alloc_node<ConstructorNode>();
				constructor->data_type = bt;

				while (true) {
					int cofs = str_ofs;
					_get_token(tk);
					if (tk.type == TK_PARENTHESIS_CLOSE) {
						break;
					}
					str_ofs = cofs; //revert
					//parse an expression
					ENode *subexpr = _parse_expression();
					if (!subexpr) {
						return nullptr;
					}

					constructor->arguments.push_back(subexpr);

					cofs = str_ofs;
					_get_token(tk);
					if (tk.type == TK_COMMA) {
						//all good
					} else if (tk.type == TK_PARENTHESIS_CLOSE) {
						str_ofs = cofs;
					} else {
						_set_error("Expected ',' or ')'");
					}
				}

				expr = constructor;

			} break;
			case TK_BUILTIN_FUNC: {
				//builtin function

				StringName func = tk.value;

				_get_token(tk);
				if (tk.type != TK_PARENTHESIS_OPEN) {
					_set_error("Expected '('");
					return nullptr;
				}

				BuiltinFuncNode *bifunc = alloc_node<BuiltinFuncNode>();
				bifunc->func = func;

				while (true) {
					int cofs = str_ofs;
					_get_token(tk);
					if (tk.type == TK_PARENTHESIS_CLOSE) {
						break;
					}
					str_ofs = cofs; //revert
					//parse an expression
					ENode *subexpr = _parse_expression();
					if (!subexpr) {
						return nullptr;
					}

					bifunc->arguments.push_back(subexpr);

					cofs = str_ofs;
					_get_token(tk);
					if (tk.type == TK_COMMA) {
						//all good
					} else if (tk.type == TK_PARENTHESIS_CLOSE) {
						str_ofs = cofs;
					} else {
						_set_error("Expected ',' or ')'");
					}
				}

				if (!Variant::is_utility_function_vararg(bifunc->func)) {
					int expected_args = Variant::get_utility_function_argument_count(bifunc->func);
					if (expected_args != bifunc->arguments.size()) {
						_set_error("Builtin func '" + String(bifunc->func) + "' expects " + itos(expected_args) + " arguments.");
					}
				}

				expr = bifunc;

			} break;
			case TK_OP_SUB: {
				ExpressionNode e;
				e.is_op = true;
				e.op = Variant::OP_NEGATE;
				expression_nodes.push_back(e);
				continue;
			} break;
			case TK_OP_NOT: {
				ExpressionNode e;
				e.is_op = true;
				e.op = Variant::OP_NOT;
				expression_nodes.push_back(e);
				continue;
			} break;

			default: {
				_set_error("Expected expression.");
				return nullptr;
			} break;
		}

		//before going to operators, must check indexing!

		while (true) {
			int cofs2 = str_ofs;
			_get_token(tk);
			if (error_set) {
				return nullptr;
			}

			bool done = false;

			switch (tk.type) {
				case TK_BRACKET_OPEN: {
					//value indexing

					IndexNode *index = alloc_node<IndexNode>();
					index->base = expr;

					ENode *what = _parse_expression();
					if (!what) {
						return nullptr;
					}

					index->index = what;

					_get_token(tk);
					if (tk.type != TK_BRACKET_CLOSE) {
						_set_error("Expected ']' at end of index.");
						return nullptr;
					}
					expr = index;

				} break;
				case TK_PERIOD: {
					//named indexing or function call
					_get_token(tk);
					if (tk.type != TK_IDENTIFIER && tk.type != TK_BUILTIN_FUNC) {
						_set_error("Expected identifier after '.'");
						return nullptr;
					}

					StringName identifier = tk.value;

					int cofs = str_ofs;
					_get_token(tk);
					if (tk.type == TK_PARENTHESIS_OPEN) {
						//function call
						CallNode *func_call = alloc_node<CallNode>();
						func_call->method = identifier;
						func_call->base = expr;

						while (true) {
							int cofs3 = str_ofs;
							_get_token(tk);
							if (tk.type == TK_PARENTHESIS_CLOSE) {
								break;
							}
							str_ofs = cofs3; //revert
							//parse an expression
							ENode *subexpr = _parse_expression();
							if (!subexpr) {
								return nullptr;
							}

							func_call->arguments.push_back(subexpr);

							cofs3 = str_ofs;
							_get_token(tk);
							if (tk.type == TK_COMMA) {
								//all good
							} else if (tk.type == TK_PARENTHESIS_CLOSE) {
								str_ofs = cofs3;
							} else {
								_set_error("Expected ',' or ')'");
							}
						}

						expr = func_call;
					} else {
						//named indexing
						str_ofs = cofs;

						NamedIndexNode *index = alloc_node<NamedIndexNode>();
						index->base = expr;
						index->name = identifier;
						expr = index;
					}

				} break;
				default: {
					str_ofs = cofs2;
					done = true;
				} break;
			}

			if (done) {
				break;
			}
		}

		//push expression
		{
			ExpressionNode e;
			e.is_op = false;
			e.node = expr;
			expression_nodes.push_back(e);
		}

		//ok finally look for an operator

		int cofs = str_ofs;
		_get_token(tk);
		if (error_set) {
			return nullptr;
		}

		Variant::Operator op = Variant::OP_MAX;

		switch (tk.type) {
			case TK_OP_IN:
				op = Variant::OP_IN;
				break;
			case TK_OP_EQUAL:
				op = Variant::OP_EQUAL;
				break;
			case TK_OP_NOT_EQUAL:
				op = Variant::OP_NOT_EQUAL;
				break;
			case TK_OP_LESS:
				op = Variant::OP_LESS;
				break;
			case TK_OP_LESS_EQUAL:
				op = Variant::OP_LESS_EQUAL;
				break;
			case TK_OP_GREATER:
				op = Variant::OP_GREATER;
				break;
			case TK_OP_GREATER_EQUAL:
				op = Variant::OP_GREATER_EQUAL;
				break;
			case TK_OP_AND:
				op = Variant::OP_AND;
				break;
			case TK_OP_OR:
				op = Variant::OP_OR;
				break;
			case TK_OP_NOT:
				op = Variant::OP_NOT;
				break;
			case TK_OP_ADD:
				op = Variant::OP_ADD;
				break;
			case TK_OP_SUB:
				op = Variant::OP_SUBTRACT;
				break;
			case TK_OP_MUL:
				op = Variant::OP_MULTIPLY;
				break;
			case TK_OP_DIV:
				op = Variant::OP_DIVIDE;
				break;
			case TK_OP_MOD:
				op = Variant::OP_MODULE;
				break;
			case TK_OP_POW:
				op = Variant::OP_POWER;
				break;
			case TK_OP_SHIFT_LEFT:
				op = Variant::OP_SHIFT_LEFT;
				break;
			case TK_OP_SHIFT_RIGHT:
				op = Variant::OP_SHIFT_RIGHT;
				break;
			case TK_OP_BIT_AND:
				op = Variant::OP_BIT_AND;
				break;
			case TK_OP_BIT_OR:
				op = Variant::OP_BIT_OR;
				break;
			case TK_OP_BIT_XOR:
				op = Variant::OP_BIT_XOR;
				break;
			case TK_OP_BIT_INVERT:
				op = Variant::OP_BIT_NEGATE;
				break;
			default: {
			}
		}

		if (op == Variant::OP_MAX) { //stop appending stuff
			str_ofs = cofs;
			break;
		}

		//push operator and go on
		{
			ExpressionNode e;
			e.is_op = true;
			e.op = op;
			expression_nodes.push_back(e);
		}
	}

	/* Reduce the set of expressions and place them in an operator tree, respecting precedence */

	while (expression_nodes.size() > 1) {
		int next_op = -1;
		int min_priority = 0xFFFFF;
		bool is_unary = false;

		for (int i = 0; i < expression_nodes.size(); i++) {
			if (!expression_nodes[i].is_op) {
				continue;
			}

			int priority;

			bool unary = false;

			switch (expression_nodes[i].op) {
				case Variant::OP_POWER:
					priority = 0;
					break;
				case Variant::OP_BIT_NEGATE:
					priority = 1;
					unary = true;
					break;
				case Variant::OP_NEGATE:
					priority = 2;
					unary = true;
					break;
				case Variant::OP_MULTIPLY:
				case Variant::OP_DIVIDE:
				case Variant::OP_MODULE:
					priority = 3;
					break;
				case Variant::OP_ADD:
				case Variant::OP_SUBTRACT:
					priority = 4;
					break;
				case Variant::OP_SHIFT_LEFT:
				case Variant::OP_SHIFT_RIGHT:
					priority = 5;
					break;
				case Variant::OP_BIT_AND:
					priority = 6;
					break;
				case Variant::OP_BIT_XOR:
					priority = 7;
					break;
				case Variant::OP_BIT_OR:
					priority = 8;
					break;
				case Variant::OP_LESS:
				case Variant::OP_LESS_EQUAL:
				case Variant::OP_GREATER:
				case Variant::OP_GREATER_EQUAL:
				case Variant::OP_EQUAL:
				case Variant::OP_NOT_EQUAL:
					priority = 9;
					break;
				case Variant::OP_IN:
					priority = 11;
					break;
				case Variant::OP_NOT:
					priority = 12;
					unary = true;
					break;
				case Variant::OP_AND:
					priority = 13;
					break;
				case Variant::OP_OR:
					priority = 14;
					break;
				default: {
					_set_error("Parser bug, invalid operator in expression: " + itos(expression_nodes[i].op));
					return nullptr;
				}
			}

			if (priority < min_priority) {
				// < is used for left to right (default)
				// <= is used for right to left

				next_op = i;
				min_priority = priority;
				is_unary = unary;
			}
		}

		if (next_op == -1) {
			_set_error("Yet another parser bug....");
			ERR_FAIL_V(nullptr);
		}

		// OK! create operator..
		if (is_unary) {
			int expr_pos = next_op;
			while (expression_nodes[expr_pos].is_op) {
				expr_pos++;
				if (expr_pos == expression_nodes.size()) {
					//can happen..
					_set_error("Unexpected end of expression...");
					return nullptr;
				}
			}

			//consecutively do unary operators
			for (int i = expr_pos - 1; i >= next_op; i--) {
				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = expression_nodes[i].op;
				op->nodes[0] = expression_nodes[i + 1].node;
				op->nodes[1] = nullptr;
				expression_nodes.write[i].is_op = false;
				expression_nodes.write[i].node = op;
				expression_nodes.remove_at(i + 1);
			}

		} else {
			if (next_op < 1 || next_op >= (expression_nodes.size() - 1)) {
				_set_error("Parser bug...");
				ERR_FAIL_V(nullptr);
			}

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = expression_nodes[next_op].op;

			if (expression_nodes[next_op - 1].is_op) {
				_set_error("Parser bug...");
				ERR_FAIL_V(nullptr);
			}

			if (expression_nodes[next_op + 1].is_op) {
				// this is not invalid and can really appear
				// but it becomes invalid anyway because no binary op
				// can be followed by a unary op in a valid combination,
				// due to how precedence works, unaries will always disappear first

				_set_error("Unexpected two consecutive operators.");
				return nullptr;
			}

			op->nodes[0] = expression_nodes[next_op - 1].node; //expression goes as left
			op->nodes[1] = expression_nodes[next_op + 1].node; //next expression goes as right

			//replace all 3 nodes by this operator and make it an expression
			expression_nodes.write[next_op - 1].node = op;
			expression_nodes.remove_at(next_op);
			expression_nodes.remove_at(next_op);
		}
	}

	return expression_nodes[0].node;
}

bool Expression::_compile_expression() {
	if (!expression_dirty) {
		return error_set;
	}

	if (nodes) {
		memdelete(nodes);
		nodes = nullptr;
		root = nullptr;
	}

	error_str = String();
	error_set = false;
	str_ofs = 0;

	root = _parse_expression();

	if (error_set) {
		root = nullptr;
		if (nodes) {
			memdelete(nodes);
		}
		nodes = nullptr;
		return true;
	}

	expression_dirty = false;
	return false;
}

bool Expression::_execute(const Array &p_inputs, Object *p_instance, Expression::ENode *p_node, Variant &r_ret, bool p_const_calls_only, String &r_error_str) {
	switch (p_node->type) {
		case Expression::ENode::TYPE_INPUT: {
			const Expression::InputNode *in = static_cast<const Expression::InputNode *>(p_node);
			if (in->index < 0 || in->index >= p_inputs.size()) {
				r_error_str = vformat(RTR("Invalid input %d (not passed) in expression"), in->index);
				return true;
			}
			r_ret = p_inputs[in->index];
		} break;
		case Expression::ENode::TYPE_CONSTANT: {
			const Expression::ConstantNode *c = static_cast<const Expression::ConstantNode *>(p_node);
			r_ret = c->value;

		} break;
		case Expression::ENode::TYPE_SELF: {
			if (!p_instance) {
				r_error_str = RTR("self can't be used because instance is null (not passed)");
				return true;
			}
			r_ret = p_instance;
		} break;
		case Expression::ENode::TYPE_OPERATOR: {
			const Expression::OperatorNode *op = static_cast<const Expression::OperatorNode *>(p_node);

			Variant a;
			bool ret = _execute(p_inputs, p_instance, op->nodes[0], a, p_const_calls_only, r_error_str);
			if (ret) {
				return true;
			}

			Variant b;

			if (op->nodes[1]) {
				ret = _execute(p_inputs, p_instance, op->nodes[1], b, p_const_calls_only, r_error_str);
				if (ret) {
					return true;
				}
			}

			bool valid = true;
			Variant::evaluate(op->op, a, b, r_ret, valid);
			if (!valid) {
				r_error_str = vformat(RTR("Invalid operands to operator %s, %s and %s."), Variant::get_operator_name(op->op), Variant::get_type_name(a.get_type()), Variant::get_type_name(b.get_type()));
				return true;
			}

		} break;
		case Expression::ENode::TYPE_INDEX: {
			const Expression::IndexNode *index = static_cast<const Expression::IndexNode *>(p_node);

			Variant base;
			bool ret = _execute(p_inputs, p_instance, index->base, base, p_const_calls_only, r_error_str);
			if (ret) {
				return true;
			}

			Variant idx;

			ret = _execute(p_inputs, p_instance, index->index, idx, p_const_calls_only, r_error_str);
			if (ret) {
				return true;
			}

			bool valid;
			r_ret = base.get(idx, &valid);
			if (!valid) {
				r_error_str = vformat(RTR("Invalid index of type %s for base type %s"), Variant::get_type_name(idx.get_type()), Variant::get_type_name(base.get_type()));
				return true;
			}

		} break;
		case Expression::ENode::TYPE_NAMED_INDEX: {
			const Expression::NamedIndexNode *index = static_cast<const Expression::NamedIndexNode *>(p_node);

			Variant base;
			bool ret = _execute(p_inputs, p_instance, index->base, base, p_const_calls_only, r_error_str);
			if (ret) {
				return true;
			}

			bool valid;
			r_ret = base.get_named(index->name, valid);
			if (!valid) {
				r_error_str = vformat(RTR("Invalid named index '%s' for base type %s"), String(index->name), Variant::get_type_name(base.get_type()));
				return true;
			}

		} break;
		case Expression::ENode::TYPE_ARRAY: {
			const Expression::ArrayNode *array = static_cast<const Expression::ArrayNode *>(p_node);

			Array arr;
			arr.resize(array->array.size());
			for (int i = 0; i < array->array.size(); i++) {
				Variant value;
				bool ret = _execute(p_inputs, p_instance, array->array[i], value, p_const_calls_only, r_error_str);

				if (ret) {
					return true;
				}
				arr[i] = value;
			}

			r_ret = arr;

		} break;
		case Expression::ENode::TYPE_DICTIONARY: {
			const Expression::DictionaryNode *dictionary = static_cast<const Expression::DictionaryNode *>(p_node);

			Dictionary d;
			for (int i = 0; i < dictionary->dict.size(); i += 2) {
				Variant key;
				bool ret = _execute(p_inputs, p_instance, dictionary->dict[i + 0], key, p_const_calls_only, r_error_str);

				if (ret) {
					return true;
				}

				Variant value;
				ret = _execute(p_inputs, p_instance, dictionary->dict[i + 1], value, p_const_calls_only, r_error_str);
				if (ret) {
					return true;
				}

				d[key] = value;
			}

			r_ret = d;
		} break;
		case Expression::ENode::TYPE_CONSTRUCTOR: {
			const Expression::ConstructorNode *constructor = static_cast<const Expression::ConstructorNode *>(p_node);

			Vector<Variant> arr;
			Vector<const Variant *> argp;
			arr.resize(constructor->arguments.size());
			argp.resize(constructor->arguments.size());

			for (int i = 0; i < constructor->arguments.size(); i++) {
				Variant value;
				bool ret = _execute(p_inputs, p_instance, constructor->arguments[i], value, p_const_calls_only, r_error_str);

				if (ret) {
					return true;
				}
				arr.write[i] = value;
				argp.write[i] = &arr[i];
			}

			Callable::CallError ce;
			Variant::construct(constructor->data_type, r_ret, (const Variant **)argp.ptr(), argp.size(), ce);

			if (ce.error != Callable::CallError::CALL_OK) {
				r_error_str = vformat(RTR("Invalid arguments to construct '%s'"), Variant::get_type_name(constructor->data_type));
				return true;
			}

		} break;
		case Expression::ENode::TYPE_BUILTIN_FUNC: {
			const Expression::BuiltinFuncNode *bifunc = static_cast<const Expression::BuiltinFuncNode *>(p_node);

			Vector<Variant> arr;
			Vector<const Variant *> argp;
			arr.resize(bifunc->arguments.size());
			argp.resize(bifunc->arguments.size());

			for (int i = 0; i < bifunc->arguments.size(); i++) {
				Variant value;
				bool ret = _execute(p_inputs, p_instance, bifunc->arguments[i], value, p_const_calls_only, r_error_str);
				if (ret) {
					return true;
				}
				arr.write[i] = value;
				argp.write[i] = &arr[i];
			}

			r_ret = Variant(); //may not return anything
			Callable::CallError ce;
			Variant::call_utility_function(bifunc->func, &r_ret, (const Variant **)argp.ptr(), argp.size(), ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				r_error_str = "Builtin call failed: " + Variant::get_call_error_text(bifunc->func, (const Variant **)argp.ptr(), argp.size(), ce);
				return true;
			}

		} break;
		case Expression::ENode::TYPE_CALL: {
			const Expression::CallNode *call = static_cast<const Expression::CallNode *>(p_node);

			Variant base;
			bool ret = _execute(p_inputs, p_instance, call->base, base, p_const_calls_only, r_error_str);

			if (ret) {
				return true;
			}

			Vector<Variant> arr;
			Vector<const Variant *> argp;
			arr.resize(call->arguments.size());
			argp.resize(call->arguments.size());

			for (int i = 0; i < call->arguments.size(); i++) {
				Variant value;
				ret = _execute(p_inputs, p_instance, call->arguments[i], value, p_const_calls_only, r_error_str);

				if (ret) {
					return true;
				}
				arr.write[i] = value;
				argp.write[i] = &arr[i];
			}

			Callable::CallError ce;
			if (p_const_calls_only) {
				base.call_const(call->method, (const Variant **)argp.ptr(), argp.size(), r_ret, ce);
			} else {
				base.callp(call->method, (const Variant **)argp.ptr(), argp.size(), r_ret, ce);
			}

			if (ce.error != Callable::CallError::CALL_OK) {
				r_error_str = vformat(RTR("On call to '%s':"), String(call->method));
				return true;
			}

		} break;
	}
	return false;
}

Error Expression::parse(const String &p_expression, const Vector<String> &p_input_names) {
	if (nodes) {
		memdelete(nodes);
		nodes = nullptr;
		root = nullptr;
	}

	error_str = String();
	error_set = false;
	str_ofs = 0;
	input_names = p_input_names;

	expression = p_expression;
	root = _parse_expression();

	if (error_set) {
		root = nullptr;
		if (nodes) {
			memdelete(nodes);
		}
		nodes = nullptr;
		return ERR_INVALID_PARAMETER;
	}

	return OK;
}

Variant Expression::execute(const Array &p_inputs, Object *p_base, bool p_show_error, bool p_const_calls_only) {
	ERR_FAIL_COND_V_MSG(error_set, Variant(), vformat("There was previously a parse error: %s.", error_str));

	execution_error = false;
	Variant output;
	String error_txt;
	bool err = _execute(p_inputs, p_base, root, output, p_const_calls_only, error_txt);
	if (err) {
		execution_error = true;
		error_str = error_txt;
		ERR_FAIL_COND_V_MSG(p_show_error, Variant(), error_str);
	}

	return output;
}

bool Expression::has_execute_failed() const {
	return execution_error;
}

String Expression::get_error_text() const {
	return error_str;
}

void Expression::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "expression", "input_names"), &Expression::parse, DEFVAL(Vector<String>()));
	ClassDB::bind_method(D_METHOD("execute", "inputs", "base_instance", "show_error", "const_calls_only"), &Expression::execute, DEFVAL(Array()), DEFVAL(Variant()), DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("has_execute_failed"), &Expression::has_execute_failed);
	ClassDB::bind_method(D_METHOD("get_error_text"), &Expression::get_error_text);
}

Expression::~Expression() {
	if (nodes) {
		memdelete(nodes);
	}
}
