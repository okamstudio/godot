/**************************************************************************/
/*  variant_parser.cpp                                                    */
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

#include "variant_parser.h"

#include "core/io/resource_loader.h"
#include "core/os/input_event.h"
#include "core/os/keyboard.h"
#include "core/string_buffer.h"

// Keep this function non-virtual, and as optimized as possible for the common case.
CharType VariantParser::Stream::get_char() {
	// is within buffer?
	if (readahead_pointer < readahead_filled) {
		return readahead_buffer[readahead_pointer++];
	}

	// attempt to readahead
	readahead_filled = _read_buffer(readahead_buffer, readahead_size);
	if (readahead_filled) {
		readahead_pointer = 0;
	} else {
		// EOF
		readahead_pointer = 1;
		_eof = true;
		return 0;
	}
	return get_char();
}

bool VariantParser::Stream::is_eof() const {
	return _eof;
}

uint32_t VariantParser::Stream::get_readahead_offset() const {
	DEV_ASSERT(!_eof || ((readahead_filled - readahead_pointer) == 0));
	// Uncomment the next line if the assert is being hit.
	// if (_eof) {return 0;}
	return readahead_filled - readahead_pointer;
}

void VariantParser::Stream::invalidate_readahead(bool p_eof) {
	readahead_filled = 0;
	readahead_pointer = p_eof ? 1 : 0;
	_eof = p_eof;
}

Error VariantParser::StreamFile::open_file(const String &p_path) {
	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

	if (!f) {
		// double check we are not sending an OK in case of failure.
		if (err == OK) {
			err = FAILED;
		}
		return err;
	}

	if (err == OK) {
		// Always use readahead for owned files.
		// This is always safe, because the file pointer etc cannot be changed
		// externally, and the chief reason for having owned files.
		set_file(f, READAHEAD_ENABLED);
		_is_owned_file = true;
	}

	return err;
}

Error VariantParser::StreamFile::close_file() {
	if (_is_owned_file) {
		if (_file) {
			memdelete(_file);
			_file = nullptr;
			_is_owned_file = false;
			invalidate_readahead();
			return OK;
		}
		// This should probably never occur, but just in case...
		_is_owned_file = false;
	}

	return FAILED;
}

VariantParser::StreamFile::~StreamFile() {
	// free any owned file
	close_file();
}

void VariantParser::StreamFile::set_file(FileAccess *p_file, Readahead p_readahead) {
	// free any existing owned file
	close_file();

	bool use_readahead = p_readahead == READAHEAD_ENABLED;

	// noop?
	if ((p_file == _file) && (use_readahead == readahead_enabled())) {
		return;
	}

	_file = p_file;

	// set_file defaults to a non-owned file,
	// if we are setting an owned file we must set this bool
	// immediately after set_file().
	_is_owned_file = false;

	readahead_size = use_readahead ? READAHEAD_SIZE : 1;
	invalidate_readahead();

	// Files should be opened BEFORE calling set_file.
	ERR_FAIL_COND(_file && !_file->is_open());
}

void VariantParser::StreamFile::invalidate_readahead() {
	bool eof = true;
	if (_file) {
		bool open = _file->is_open();
		eof = open ? _file->eof_reached() : false;

		VariantParser::Stream::invalidate_readahead(eof);
#ifdef DEV_ENABLED
		_readahead_start_source_pos = open ? _file->get_position() : 0;
		_readahead_end_source_pos = _readahead_start_source_pos;
#endif
	} else {
		VariantParser::Stream::invalidate_readahead(true);
#ifdef DEV_ENABLED
		_readahead_start_source_pos = 0;
		_readahead_end_source_pos = 0;
#endif
	}
}

uint32_t VariantParser::StreamFile::_read_buffer(CharType *p_buffer, uint32_t p_num_chars) {
	// The buffer is assumed to include at least one character (for null terminator)
	DEV_ASSERT(p_num_chars);
	ERR_FAIL_NULL_V_MSG(_file, 0, "Attempting to read from NULL file.");

#ifdef DEV_ENABLED
	if (readahead_enabled()) {
		_readahead_start_source_pos = _file->get_position();
		if (_readahead_start_source_pos != _readahead_end_source_pos) {
			ERR_PRINT_ONCE("StreamFile out of sync. HIGH PRIORITY bug, please report on github.");
			return 0;
		}
	}
#endif

	uint8_t *temp = (uint8_t *)alloca(p_num_chars);
	uint64_t num_read = _file->get_buffer(temp, p_num_chars);

#ifdef DEV_ENABLED
	if (readahead_enabled()) {
		_readahead_end_source_pos = _file->get_position();
	}
#endif

	ERR_FAIL_COND_V(num_read == UINT64_MAX, 0);

	// translate to wchar
	for (uint32_t n = 0; n < num_read; n++) {
		p_buffer[n] = temp[n];
	}

	// could be less than p_num_chars, or zero
	return num_read;
}

bool VariantParser::StreamFile::is_utf8() const {
	return true;
}

bool VariantParser::StreamFile::_is_eof() const {
	ERR_FAIL_NULL_V(_file, true);
	return _file->eof_reached();
}

uint64_t VariantParser::StreamFile::get_position() const {
	ERR_FAIL_NULL_V(_file, -1);

	// Note this assumes that get_position() returns get_length() when EOF
	// is reached. Watch for bugs here.
	return _file->get_position() - get_readahead_offset();
}

uint64_t VariantParser::StreamString::get_position() const {
	return _pos - get_readahead_offset();
}

uint32_t VariantParser::StreamString::_read_buffer(CharType *p_buffer, uint32_t p_num_chars) {
	// The buffer is assumed to include at least one character (for null terminator)
	DEV_ASSERT(p_num_chars);

	int available = MAX(s.length() - _pos, 0);
	if (available >= (int)p_num_chars) {
		const CharType *src = s.ptr();
		src += _pos;
		memcpy(p_buffer, src, p_num_chars * sizeof(CharType));
		_pos += p_num_chars;

		return p_num_chars;
	}

	// going to reach EOF
	if (available) {
		const CharType *src = s.ptr();
		src += _pos;
		memcpy(p_buffer, src, available * sizeof(CharType));
		_pos += available;
	}

	// add a zero
	p_buffer[available] = 0;

	return available;
}

bool VariantParser::StreamString::is_utf8() const {
	return false;
}

bool VariantParser::StreamString::_is_eof() const {
	return _pos > s.length();
}

/////////////////////////////////////////////////////////////////////////////////////////////////

const char *VariantParser::tk_name[TK_MAX] = {
	"'{'",
	"'}'",
	"'['",
	"']'",
	"'('",
	"')'",
	"identifier",
	"string",
	"number",
	"color",
	"':'",
	"','",
	"'.'",
	"'='",
	"EOF",
	"ERROR"
};

static double stor_fix(const String &p_str) {
	if (p_str == "inf") {
		return Math_INF;
	} else if (p_str == "inf_neg") {
		return -Math_INF;
	} else if (p_str == "nan") {
		return Math_NAN;
	}
	return -1;
}

Error VariantParser::get_token(Stream *p_stream, Token &r_token, int &line, String &r_err_str) {
	while (true) {
		CharType cchar;
		if (p_stream->saved) {
			cchar = p_stream->saved;
			p_stream->saved = 0;
		} else {
			cchar = p_stream->get_char();
			if (p_stream->is_eof()) {
				r_token.type = TK_EOF;
				return OK;
			}
		}

		switch (cchar) {
			case '\n': {
				line++;
				break;
			};
			case 0: {
				r_token.type = TK_EOF;
				return OK;
			} break;
			case '{': {
				r_token.type = TK_CURLY_BRACKET_OPEN;
				return OK;
			};
			case '}': {
				r_token.type = TK_CURLY_BRACKET_CLOSE;
				return OK;
			};
			case '[': {
				r_token.type = TK_BRACKET_OPEN;
				return OK;
			};
			case ']': {
				r_token.type = TK_BRACKET_CLOSE;
				return OK;
			};
			case '(': {
				r_token.type = TK_PARENTHESIS_OPEN;
				return OK;
			};
			case ')': {
				r_token.type = TK_PARENTHESIS_CLOSE;
				return OK;
			};
			case ':': {
				r_token.type = TK_COLON;
				return OK;
			};
			case ';': {
				while (true) {
					CharType ch = p_stream->get_char();
					if (p_stream->is_eof()) {
						r_token.type = TK_EOF;
						return OK;
					}
					if (ch == '\n') {
						line++;
						break;
					}
				}

				break;
			};
			case ',': {
				r_token.type = TK_COMMA;
				return OK;
			};
			case '.': {
				r_token.type = TK_PERIOD;
				return OK;
			};
			case '=': {
				r_token.type = TK_EQUAL;
				return OK;
			};
			case '#': {
				StringBuffer<> color_str;
				color_str += '#';
				while (true) {
					CharType ch = p_stream->get_char();
					if (p_stream->is_eof()) {
						r_token.type = TK_EOF;
						return OK;
					} else if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
						color_str += ch;

					} else {
						p_stream->saved = ch;
						break;
					}
				}

				r_token.value = Color::html(color_str.as_string());
				r_token.type = TK_COLOR;
				return OK;
			};
			case '"': {
				String str;
				while (true) {
					CharType ch = p_stream->get_char();

					if (ch == 0) {
						r_err_str = "Unterminated String";
						r_token.type = TK_ERROR;
						return ERR_PARSE_ERROR;
					} else if (ch == '"') {
						break;
					} else if (ch == '\\') {
						//escaped characters...
						CharType next = p_stream->get_char();
						if (next == 0) {
							r_err_str = "Unterminated String";
							r_token.type = TK_ERROR;
							return ERR_PARSE_ERROR;
						}
						CharType res = 0;

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
							case 'u': {
								//hexnumbarh - oct is deprecated

								for (int j = 0; j < 4; j++) {
									CharType c = p_stream->get_char();
									if (c == 0) {
										r_err_str = "Unterminated String";
										r_token.type = TK_ERROR;
										return ERR_PARSE_ERROR;
									}
									if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
										r_err_str = "Malformed hex constant in string";
										r_token.type = TK_ERROR;
										return ERR_PARSE_ERROR;
									}
									CharType v;
									if (c >= '0' && c <= '9') {
										v = c - '0';
									} else if (c >= 'a' && c <= 'f') {
										v = c - 'a';
										v += 10;
									} else if (c >= 'A' && c <= 'F') {
										v = c - 'A';
										v += 10;
									} else {
										ERR_PRINT("BUG");
										v = 0;
									}

									res <<= 4;
									res |= v;
								}

							} break;
							//case '\"': res='\"'; break;
							//case '\\': res='\\'; break;
							//case '/': res='/'; break;
							default: {
								res = next;
								//r_err_str="Invalid escape sequence";
								//return ERR_PARSE_ERROR;
							} break;
						}

						str += res;

					} else {
						if (ch == '\n') {
							line++;
						}
						str += ch;
					}
				}

				if (p_stream->is_utf8()) {
					str.parse_utf8(str.ascii(true).get_data());
				}
				r_token.type = TK_STRING;
				r_token.value = str;
				return OK;

			} break;
			default: {
				if (cchar <= 32) {
					break;
				}

				if (cchar == '-' || (cchar >= '0' && cchar <= '9')) {
					//a number

					StringBuffer<> num;
#define READING_SIGN 0
#define READING_INT 1
#define READING_DEC 2
#define READING_EXP 3
#define READING_DONE 4
					int reading = READING_INT;

					if (cchar == '-') {
						num += '-';
						cchar = p_stream->get_char();
					}

					CharType c = cchar;
					bool exp_sign = false;
					bool exp_beg = false;
					bool is_float = false;

					while (true) {
						switch (reading) {
							case READING_INT: {
								if (c >= '0' && c <= '9') {
									//pass
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
							case READING_DEC: {
								if (c >= '0' && c <= '9') {
								} else if (c == 'e') {
									reading = READING_EXP;
								} else {
									reading = READING_DONE;
								}

							} break;
							case READING_EXP: {
								if (c >= '0' && c <= '9') {
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
						num += c;
						c = p_stream->get_char();
					}

					p_stream->saved = c;

					r_token.type = TK_NUMBER;

					if (is_float) {
						r_token.value = num.as_double();
					} else {
						r_token.value = num.as_int();
					}
					return OK;

				} else if ((cchar >= 'A' && cchar <= 'Z') || (cchar >= 'a' && cchar <= 'z') || cchar == '_') {
					StringBuffer<> id;
					bool first = true;

					while ((cchar >= 'A' && cchar <= 'Z') || (cchar >= 'a' && cchar <= 'z') || cchar == '_' || (!first && cchar >= '0' && cchar <= '9')) {
						id += cchar;
						cchar = p_stream->get_char();
						first = false;
					}

					p_stream->saved = cchar;

					r_token.type = TK_IDENTIFIER;
					r_token.value = id.as_string();
					return OK;
				} else {
					r_err_str = "Unexpected character.";
					r_token.type = TK_ERROR;
					return ERR_PARSE_ERROR;
				}
			}
		}
	}

	r_token.type = TK_ERROR;
	return ERR_PARSE_ERROR;
}

Error VariantParser::_parse_enginecfg(Stream *p_stream, Vector<String> &strings, int &line, String &r_err_str) {
	Token token;
	get_token(p_stream, token, line, r_err_str);
	if (token.type != TK_PARENTHESIS_OPEN) {
		r_err_str = "Expected '(' in old-style project.godot construct";
		return ERR_PARSE_ERROR;
	}

	String accum;

	while (true) {
		CharType c = p_stream->get_char();

		if (p_stream->is_eof()) {
			r_err_str = "Unexpected EOF while parsing old-style project.godot construct";
			return ERR_PARSE_ERROR;
		}

		if (c == ',') {
			strings.push_back(accum.strip_edges());
			accum = String();
		} else if (c == ')') {
			strings.push_back(accum.strip_edges());
			return OK;
		} else if (c == '\n') {
			line++;
		}
	}
}

template <class T>
Error VariantParser::_parse_construct(Stream *p_stream, Vector<T> &r_construct, int &line, String &r_err_str) {
	Token token;
	get_token(p_stream, token, line, r_err_str);
	if (token.type != TK_PARENTHESIS_OPEN) {
		r_err_str = "Expected '(' in constructor";
		return ERR_PARSE_ERROR;
	}

	bool first = true;
	while (true) {
		if (!first) {
			get_token(p_stream, token, line, r_err_str);
			if (token.type == TK_COMMA) {
				//do none
			} else if (token.type == TK_PARENTHESIS_CLOSE) {
				break;
			} else {
				r_err_str = "Expected ',' or ')' in constructor";
				return ERR_PARSE_ERROR;
			}
		}
		get_token(p_stream, token, line, r_err_str);

		if (first && token.type == TK_PARENTHESIS_CLOSE) {
			break;
		} else if (token.type != TK_NUMBER) {
			bool valid = false;
			if (token.type == TK_IDENTIFIER) {
				double real = stor_fix(token.value);
				if (real != -1) {
					token.type = TK_NUMBER;
					token.value = real;
					valid = true;
				}
			}
			if (!valid) {
				r_err_str = "Expected float in constructor";
				return ERR_PARSE_ERROR;
			}
		}

		r_construct.push_back(token.value);
		first = false;
	}

	return OK;
}

Error VariantParser::parse_value(Token &token, Variant &value, Stream *p_stream, int &line, String &r_err_str, ResourceParser *p_res_parser) {
	if (token.type == TK_CURLY_BRACKET_OPEN) {
		Dictionary d;
		Error err = _parse_dictionary(d, p_stream, line, r_err_str, p_res_parser);
		if (err) {
			return err;
		}
		value = d;
		return OK;
	} else if (token.type == TK_BRACKET_OPEN) {
		Array a;
		Error err = _parse_array(a, p_stream, line, r_err_str, p_res_parser);
		if (err) {
			return err;
		}
		value = a;
		return OK;
	} else if (token.type == TK_IDENTIFIER) {
		String id = token.value;
		if (id == "true") {
			value = true;
		} else if (id == "false") {
			value = false;
		} else if (id == "null" || id == "nil") {
			value = Variant();
		} else if (id == "inf") {
			value = Math_INF;
		} else if (id == "inf_neg") {
			value = -Math_INF;
		} else if (id == "nan") {
			value = Math_NAN;
		} else if (id == "Vector2") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 2) {
				r_err_str = "Expected 2 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			value = Vector2(args[0], args[1]);
		} else if (id == "Rect2") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 4) {
				r_err_str = "Expected 4 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			value = Rect2(args[0], args[1], args[2], args[3]);
		} else if (id == "Vector3") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 3) {
				r_err_str = "Expected 3 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			value = Vector3(args[0], args[1], args[2]);
		} else if (id == "Transform2D" || id == "Matrix32") { //compatibility
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 6) {
				r_err_str = "Expected 6 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			Transform2D m;
			m[0] = Vector2(args[0], args[1]);
			m[1] = Vector2(args[2], args[3]);
			m[2] = Vector2(args[4], args[5]);
			value = m;
		} else if (id == "Plane") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 4) {
				r_err_str = "Expected 4 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			value = Plane(args[0], args[1], args[2], args[3]);
		} else if (id == "Quat") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 4) {
				r_err_str = "Expected 4 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			value = Quat(args[0], args[1], args[2], args[3]);
		} else if (id == "AABB" || id == "Rect3") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 6) {
				r_err_str = "Expected 6 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			value = AABB(Vector3(args[0], args[1], args[2]), Vector3(args[3], args[4], args[5]));
		} else if (id == "Basis" || id == "Matrix3") { //compatibility
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 9) {
				r_err_str = "Expected 9 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			value = Basis(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
		} else if (id == "Transform") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 12) {
				r_err_str = "Expected 12 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			value = Transform(Basis(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]), Vector3(args[9], args[10], args[11]));
		} else if (id == "Color") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			if (args.size() != 4) {
				r_err_str = "Expected 4 arguments for constructor";
				return ERR_PARSE_ERROR;
			}

			value = Color(args[0], args[1], args[2], args[3]);
		} else if (id == "NodePath") {
			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_PARENTHESIS_OPEN) {
				r_err_str = "Expected '('";
				return ERR_PARSE_ERROR;
			}

			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_STRING) {
				r_err_str = "Expected string as argument for NodePath()";
				return ERR_PARSE_ERROR;
			}

			value = NodePath(String(token.value));

			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_PARENTHESIS_CLOSE) {
				r_err_str = "Expected ')'";
				return ERR_PARSE_ERROR;
			}
		} else if (id == "RID") {
			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_PARENTHESIS_OPEN) {
				r_err_str = "Expected '('";
				return ERR_PARSE_ERROR;
			}

			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_NUMBER) {
				r_err_str = "Expected number as argument";
				return ERR_PARSE_ERROR;
			}

			value = token.value;

			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_PARENTHESIS_CLOSE) {
				r_err_str = "Expected ')'";
				return ERR_PARSE_ERROR;
			}
		} else if (id == "Object") {
			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_PARENTHESIS_OPEN) {
				r_err_str = "Expected '('";
				return ERR_PARSE_ERROR;
			}

			get_token(p_stream, token, line, r_err_str);

			if (token.type != TK_IDENTIFIER) {
				r_err_str = "Expected identifier with type of object";
				return ERR_PARSE_ERROR;
			}

			String type = token.value;

			Object *obj = ClassDB::instance(type);

			if (!obj) {
				r_err_str = "Can't instance Object() of type: " + type;
				return ERR_PARSE_ERROR;
			}

			REF ref = REF(Object::cast_to<Reference>(obj));

			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_COMMA) {
				r_err_str = "Expected ',' after object type";
				return ERR_PARSE_ERROR;
			}

			bool at_key = true;
			String key;
			Token token2;
			bool need_comma = false;

			while (true) {
				if (p_stream->is_eof()) {
					r_err_str = "Unexpected End of File while parsing Object()";
					return ERR_FILE_CORRUPT;
				}

				if (at_key) {
					Error err = get_token(p_stream, token2, line, r_err_str);
					if (err != OK) {
						return err;
					}

					if (token2.type == TK_PARENTHESIS_CLOSE) {
						value = ref.is_valid() ? Variant(ref) : Variant(obj);
						return OK;
					}

					if (need_comma) {
						if (token2.type != TK_COMMA) {
							r_err_str = "Expected '}' or ','";
							return ERR_PARSE_ERROR;
						} else {
							need_comma = false;
							continue;
						}
					}

					if (token2.type != TK_STRING) {
						r_err_str = "Expected property name as string";
						return ERR_PARSE_ERROR;
					}

					key = token2.value;

					err = get_token(p_stream, token2, line, r_err_str);

					if (err != OK) {
						return err;
					}
					if (token2.type != TK_COLON) {
						r_err_str = "Expected ':'";
						return ERR_PARSE_ERROR;
					}
					at_key = false;
				} else {
					Error err = get_token(p_stream, token2, line, r_err_str);
					if (err != OK) {
						return err;
					}

					Variant v;
					err = parse_value(token2, v, p_stream, line, r_err_str, p_res_parser);
					if (err) {
						return err;
					}
					obj->set(key, v);
					need_comma = true;
					at_key = true;
				}
			}
		} else if (id == "Resource" || id == "SubResource" || id == "ExtResource") {
			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_PARENTHESIS_OPEN) {
				r_err_str = "Expected '('";
				return ERR_PARSE_ERROR;
			}

			if (p_res_parser && id == "Resource" && p_res_parser->func) {
				RES res;
				Error err = p_res_parser->func(p_res_parser->userdata, p_stream, res, line, r_err_str);
				if (err) {
					return err;
				}

				value = res;
			} else if (p_res_parser && id == "ExtResource" && p_res_parser->ext_func) {
				RES res;
				Error err = p_res_parser->ext_func(p_res_parser->userdata, p_stream, res, line, r_err_str);
				if (err) {
					return err;
				}

				value = res;
			} else if (p_res_parser && id == "SubResource" && p_res_parser->sub_func) {
				RES res;
				Error err = p_res_parser->sub_func(p_res_parser->userdata, p_stream, res, line, r_err_str);
				if (err) {
					return err;
				}

				value = res;
			} else {
				get_token(p_stream, token, line, r_err_str);
				if (token.type == TK_STRING) {
					String path = token.value;
					RES res = ResourceLoader::load(path);
					if (res.is_null()) {
						r_err_str = "Can't load resource at path: '" + path + "'.";
						return ERR_PARSE_ERROR;
					}

					get_token(p_stream, token, line, r_err_str);
					if (token.type != TK_PARENTHESIS_CLOSE) {
						r_err_str = "Expected ')'";
						return ERR_PARSE_ERROR;
					}

					value = res;
				} else {
					r_err_str = "Expected string as argument for Resource().";
					return ERR_PARSE_ERROR;
				}
			}
#ifndef DISABLE_DEPRECATED
		} else if (id == "InputEvent") {
			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_PARENTHESIS_OPEN) {
				r_err_str = "Expected '('";
				return ERR_PARSE_ERROR;
			}

			get_token(p_stream, token, line, r_err_str);

			if (token.type != TK_IDENTIFIER) {
				r_err_str = "Expected identifier";
				return ERR_PARSE_ERROR;
			}

			String id2 = token.value;

			Ref<InputEvent> ie;

			if (id2 == "NONE") {
				get_token(p_stream, token, line, r_err_str);

				if (token.type != TK_PARENTHESIS_CLOSE) {
					r_err_str = "Expected ')'";
					return ERR_PARSE_ERROR;
				}

			} else if (id2 == "KEY") {
				Ref<InputEventKey> key;
				key.instance();
				ie = key;

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_COMMA) {
					r_err_str = "Expected ','";
					return ERR_PARSE_ERROR;
				}

				get_token(p_stream, token, line, r_err_str);
				if (token.type == TK_IDENTIFIER) {
					String name = token.value;
					key->set_scancode(find_keycode(name));
				} else if (token.type == TK_NUMBER) {
					key->set_scancode(token.value);
				} else {
					r_err_str = "Expected string or integer for keycode";
					return ERR_PARSE_ERROR;
				}

				get_token(p_stream, token, line, r_err_str);

				if (token.type == TK_COMMA) {
					get_token(p_stream, token, line, r_err_str);

					if (token.type != TK_IDENTIFIER) {
						r_err_str = "Expected identifier with modifier flas";
						return ERR_PARSE_ERROR;
					}

					String mods = token.value;

					if (mods.findn("C") != -1) {
						key->set_control(true);
					}
					if (mods.findn("A") != -1) {
						key->set_alt(true);
					}
					if (mods.findn("S") != -1) {
						key->set_shift(true);
					}
					if (mods.findn("M") != -1) {
						key->set_metakey(true);
					}

					get_token(p_stream, token, line, r_err_str);
					if (token.type != TK_PARENTHESIS_CLOSE) {
						r_err_str = "Expected ')'";
						return ERR_PARSE_ERROR;
					}

				} else if (token.type != TK_PARENTHESIS_CLOSE) {
					r_err_str = "Expected ')' or modifier flags.";
					return ERR_PARSE_ERROR;
				}

			} else if (id2 == "MBUTTON") {
				Ref<InputEventMouseButton> mb;
				mb.instance();
				ie = mb;

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_COMMA) {
					r_err_str = "Expected ','";
					return ERR_PARSE_ERROR;
				}

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_NUMBER) {
					r_err_str = "Expected button index";
					return ERR_PARSE_ERROR;
				}

				mb->set_button_index(token.value);

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_PARENTHESIS_CLOSE) {
					r_err_str = "Expected ')'";
					return ERR_PARSE_ERROR;
				}

			} else if (id2 == "JBUTTON") {
				Ref<InputEventJoypadButton> jb;
				jb.instance();
				ie = jb;

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_COMMA) {
					r_err_str = "Expected ','";
					return ERR_PARSE_ERROR;
				}

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_NUMBER) {
					r_err_str = "Expected button index";
					return ERR_PARSE_ERROR;
				}

				jb->set_button_index(token.value);

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_PARENTHESIS_CLOSE) {
					r_err_str = "Expected ')'";
					return ERR_PARSE_ERROR;
				}

			} else if (id2 == "JAXIS") {
				Ref<InputEventJoypadMotion> jm;
				jm.instance();
				ie = jm;

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_COMMA) {
					r_err_str = "Expected ','";
					return ERR_PARSE_ERROR;
				}

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_NUMBER) {
					r_err_str = "Expected axis index";
					return ERR_PARSE_ERROR;
				}

				jm->set_axis(token.value);

				get_token(p_stream, token, line, r_err_str);

				if (token.type != TK_COMMA) {
					r_err_str = "Expected ',' after axis index";
					return ERR_PARSE_ERROR;
				}

				get_token(p_stream, token, line, r_err_str);
				if (token.type != TK_NUMBER) {
					r_err_str = "Expected axis sign";
					return ERR_PARSE_ERROR;
				}

				jm->set_axis_value(token.value);

				get_token(p_stream, token, line, r_err_str);

				if (token.type != TK_PARENTHESIS_CLOSE) {
					r_err_str = "Expected ')' for jaxis";
					return ERR_PARSE_ERROR;
				}

			} else {
				r_err_str = "Invalid input event type.";
				return ERR_PARSE_ERROR;
			}

			value = ie;
#endif
		} else if (id == "PoolByteArray" || id == "PackedByteArray" || id == "ByteArray") {
			Vector<uint8_t> args;
			Error err = _parse_construct<uint8_t>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			PoolVector<uint8_t> arr;
			{
				int len = args.size();
				arr.resize(len);
				PoolVector<uint8_t>::Write w = arr.write();
				for (int i = 0; i < len; i++) {
					w[i] = args[i];
				}
			}

			value = arr;
		} else if (id == "PoolIntArray" || id == "PackedInt32Array" || id == "PackedInt64Array" || id == "IntArray") {
			Vector<int> args;
			Error err = _parse_construct<int>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			PoolVector<int> arr;
			{
				int len = args.size();
				arr.resize(len);
				PoolVector<int>::Write w = arr.write();
				for (int i = 0; i < len; i++) {
					w[i] = int(args[i]);
				}
			}

			value = arr;
		} else if (id == "PoolRealArray" || id == "PackedFloat32Array" || id == "PackedFloat64Array" || id == "FloatArray") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			PoolVector<float> arr;
			{
				int len = args.size();
				arr.resize(len);
				PoolVector<float>::Write w = arr.write();
				for (int i = 0; i < len; i++) {
					w[i] = args[i];
				}
			}

			value = arr;
		} else if (id == "PoolStringArray" || id == "PackedStringArray" || id == "StringArray") {
			get_token(p_stream, token, line, r_err_str);
			if (token.type != TK_PARENTHESIS_OPEN) {
				r_err_str = "Expected '('";
				return ERR_PARSE_ERROR;
			}

			Vector<String> cs;

			bool first = true;
			while (true) {
				if (!first) {
					get_token(p_stream, token, line, r_err_str);
					if (token.type == TK_COMMA) {
						//do none
					} else if (token.type == TK_PARENTHESIS_CLOSE) {
						break;
					} else {
						r_err_str = "Expected ',' or ')'";
						return ERR_PARSE_ERROR;
					}
				}
				get_token(p_stream, token, line, r_err_str);

				if (token.type == TK_PARENTHESIS_CLOSE) {
					break;
				} else if (token.type != TK_STRING) {
					r_err_str = "Expected string";
					return ERR_PARSE_ERROR;
				}

				first = false;
				cs.push_back(token.value);
			}

			PoolVector<String> arr;
			{
				int len = cs.size();
				arr.resize(len);
				PoolVector<String>::Write w = arr.write();
				for (int i = 0; i < len; i++) {
					w[i] = cs[i];
				}
			}

			value = arr;
		} else if (id == "PoolVector2Array" || id == "PackedVector2Array" || id == "Vector2Array") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			PoolVector<Vector2> arr;
			{
				int len = args.size() / 2;
				arr.resize(len);
				PoolVector<Vector2>::Write w = arr.write();
				for (int i = 0; i < len; i++) {
					w[i] = Vector2(args[i * 2 + 0], args[i * 2 + 1]);
				}
			}

			value = arr;
		} else if (id == "PoolVector3Array" || id == "PackedVector3Array" || id == "Vector3Array") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			PoolVector<Vector3> arr;
			{
				int len = args.size() / 3;
				arr.resize(len);
				PoolVector<Vector3>::Write w = arr.write();
				for (int i = 0; i < len; i++) {
					w[i] = Vector3(args[i * 3 + 0], args[i * 3 + 1], args[i * 3 + 2]);
				}
			}

			value = arr;
		} else if (id == "PoolColorArray" || id == "PackedColorArray" || id == "ColorArray") {
			Vector<float> args;
			Error err = _parse_construct<float>(p_stream, args, line, r_err_str);
			if (err) {
				return err;
			}

			PoolVector<Color> arr;
			{
				int len = args.size() / 4;
				arr.resize(len);
				PoolVector<Color>::Write w = arr.write();
				for (int i = 0; i < len; i++) {
					w[i] = Color(args[i * 4 + 0], args[i * 4 + 1], args[i * 4 + 2], args[i * 4 + 3]);
				}
			}

			value = arr;
		} else {
			r_err_str = "Unexpected identifier: '" + id + "'.";
			return ERR_PARSE_ERROR;
		}

		// All above branches end up here unless they had an early return.
		return OK;
	} else if (token.type == TK_NUMBER) {
		value = token.value;
		return OK;
	} else if (token.type == TK_STRING) {
		value = token.value;
		return OK;
	} else if (token.type == TK_COLOR) {
		value = token.value;
		return OK;
	} else {
		r_err_str = "Expected value, got " + String(tk_name[token.type]) + ".";
		return ERR_PARSE_ERROR;
	}
}

Error VariantParser::_parse_array(Array &array, Stream *p_stream, int &line, String &r_err_str, ResourceParser *p_res_parser) {
	Token token;
	bool need_comma = false;

	while (true) {
		if (p_stream->is_eof()) {
			r_err_str = "Unexpected End of File while parsing array";
			return ERR_FILE_CORRUPT;
		}

		Error err = get_token(p_stream, token, line, r_err_str);
		if (err != OK) {
			return err;
		}

		if (token.type == TK_BRACKET_CLOSE) {
			return OK;
		}

		if (need_comma) {
			if (token.type != TK_COMMA) {
				r_err_str = "Expected ','";
				return ERR_PARSE_ERROR;
			} else {
				need_comma = false;
				continue;
			}
		}

		Variant v;
		err = parse_value(token, v, p_stream, line, r_err_str, p_res_parser);
		if (err) {
			return err;
		}

		array.push_back(v);
		need_comma = true;
	}
}

Error VariantParser::_parse_dictionary(Dictionary &object, Stream *p_stream, int &line, String &r_err_str, ResourceParser *p_res_parser) {
	bool at_key = true;
	Variant key;
	Token token;
	bool need_comma = false;

	while (true) {
		if (p_stream->is_eof()) {
			r_err_str = "Unexpected End of File while parsing dictionary";
			return ERR_FILE_CORRUPT;
		}

		if (at_key) {
			Error err = get_token(p_stream, token, line, r_err_str);
			if (err != OK) {
				return err;
			}

			if (token.type == TK_CURLY_BRACKET_CLOSE) {
				return OK;
			}

			if (need_comma) {
				if (token.type != TK_COMMA) {
					r_err_str = "Expected '}' or ','";
					return ERR_PARSE_ERROR;
				} else {
					need_comma = false;
					continue;
				}
			}

			err = parse_value(token, key, p_stream, line, r_err_str, p_res_parser);

			if (err) {
				return err;
			}

			err = get_token(p_stream, token, line, r_err_str);

			if (err != OK) {
				return err;
			}
			if (token.type != TK_COLON) {
				r_err_str = "Expected ':'";
				return ERR_PARSE_ERROR;
			}
			at_key = false;
		} else {
			Error err = get_token(p_stream, token, line, r_err_str);
			if (err != OK) {
				return err;
			}

			Variant v;
			err = parse_value(token, v, p_stream, line, r_err_str, p_res_parser);
			if (err) {
				return err;
			}
			object[key] = v;
			need_comma = true;
			at_key = true;
		}
	}
}

Error VariantParser::_parse_tag(Token &token, Stream *p_stream, int &line, String &r_err_str, Tag &r_tag, ResourceParser *p_res_parser, bool p_simple_tag) {
	r_tag.fields.clear();

	if (token.type != TK_BRACKET_OPEN) {
		r_err_str = "Expected '['";
		return ERR_PARSE_ERROR;
	}

	if (p_simple_tag) {
		r_tag.name = "";
		r_tag.fields.clear();

		if (p_stream->is_utf8()) {
			CharString cs;
			while (true) {
				CharType c = p_stream->get_char();
				if (p_stream->is_eof()) {
					r_err_str = "Unexpected EOF while parsing simple tag";
					return ERR_PARSE_ERROR;
				}
				if (c == ']') {
					break;
				}
				cs += c;
			}
			r_tag.name.parse_utf8(cs.get_data(), cs.length());
		} else {
			while (true) {
				CharType c = p_stream->get_char();
				if (p_stream->is_eof()) {
					r_err_str = "Unexpected EOF while parsing simple tag";
					return ERR_PARSE_ERROR;
				}
				if (c == ']') {
					break;
				}
				r_tag.name += String::chr(c);
			}
		}

		r_tag.name = r_tag.name.strip_edges();

		return OK;
	}

	get_token(p_stream, token, line, r_err_str);

	if (token.type != TK_IDENTIFIER) {
		r_err_str = "Expected identifier (tag name)";
		return ERR_PARSE_ERROR;
	}

	r_tag.name = token.value;
	bool parsing_tag = true;

	while (true) {
		if (p_stream->is_eof()) {
			r_err_str = "Unexpected End of File while parsing tag: " + r_tag.name;
			return ERR_FILE_CORRUPT;
		}

		get_token(p_stream, token, line, r_err_str);
		if (token.type == TK_BRACKET_CLOSE) {
			break;
		}

		if (parsing_tag && token.type == TK_PERIOD) {
			r_tag.name += "."; //support tags such as [someprop.Android] for specific platforms
			get_token(p_stream, token, line, r_err_str);
		} else if (parsing_tag && token.type == TK_COLON) {
			r_tag.name += ":"; //support tags such as [someprop.Android] for specific platforms
			get_token(p_stream, token, line, r_err_str);
		} else {
			parsing_tag = false;
		}

		if (token.type != TK_IDENTIFIER) {
			r_err_str = "Expected Identifier";
			return ERR_PARSE_ERROR;
		}

		String id = token.value;

		if (parsing_tag) {
			r_tag.name += id;
			continue;
		}

		get_token(p_stream, token, line, r_err_str);
		if (token.type != TK_EQUAL) {
			return ERR_PARSE_ERROR;
		}

		get_token(p_stream, token, line, r_err_str);
		Variant value;
		Error err = parse_value(token, value, p_stream, line, r_err_str, p_res_parser);
		if (err) {
			return err;
		}

		r_tag.fields[id] = value;
	}

	return OK;
}

Error VariantParser::parse_tag(Stream *p_stream, int &line, String &r_err_str, Tag &r_tag, ResourceParser *p_res_parser, bool p_simple_tag) {
	Token token;
	get_token(p_stream, token, line, r_err_str);

	if (token.type == TK_EOF) {
		return ERR_FILE_EOF;
	}

	if (token.type != TK_BRACKET_OPEN) {
		r_err_str = "Expected '['";
		return ERR_PARSE_ERROR;
	}

	return _parse_tag(token, p_stream, line, r_err_str, r_tag, p_res_parser, p_simple_tag);
}

Error VariantParser::parse_tag_assign_eof(Stream *p_stream, int &line, String &r_err_str, Tag &r_tag, String &r_assign, Variant &r_value, ResourceParser *p_res_parser, bool p_simple_tag) {
	//assign..
	r_assign = "";
	String what;

	while (true) {
		CharType c;
		if (p_stream->saved) {
			c = p_stream->saved;
			p_stream->saved = 0;

		} else {
			c = p_stream->get_char();
		}

		if (p_stream->is_eof()) {
			return ERR_FILE_EOF;
		}

		if (c == ';') { //comment
			while (true) {
				CharType ch = p_stream->get_char();
				if (p_stream->is_eof()) {
					return ERR_FILE_EOF;
				}
				if (ch == '\n') {
					line++;
					break;
				}
			}
			continue;
		}

		if (c == '[' && what.length() == 0) {
			//it's a tag!
			p_stream->saved = '['; //go back one

			Error err = parse_tag(p_stream, line, r_err_str, r_tag, p_res_parser, p_simple_tag);

			return err;
		}

		if (c > 32) {
			if (c == '"') { //quoted
				p_stream->saved = '"';
				Token tk;
				Error err = get_token(p_stream, tk, line, r_err_str);
				if (err) {
					return err;
				}
				if (tk.type != TK_STRING) {
					r_err_str = "Error reading quoted string";
					return ERR_INVALID_DATA;
				}

				what = tk.value;

			} else if (c != '=') {
				what += String::chr(c);
			} else {
				r_assign = what;
				Token token;
				get_token(p_stream, token, line, r_err_str);
				Error err = parse_value(token, r_value, p_stream, line, r_err_str, p_res_parser);
				return err;
			}
		} else if (c == '\n') {
			line++;
		}
	}
}

Error VariantParser::parse(Stream *p_stream, Variant &r_ret, String &r_err_str, int &r_err_line, ResourceParser *p_res_parser) {
	Token token;
	Error err = get_token(p_stream, token, r_err_line, r_err_str);
	if (err) {
		return err;
	}

	if (token.type == TK_EOF) {
		return ERR_FILE_EOF;
	}

	return parse_value(token, r_ret, p_stream, r_err_line, r_err_str, p_res_parser);
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

static String rtos_fix(double p_value) {
	if (p_value == 0.0) {
		return "0"; //avoid negative zero (-0) being written, which may annoy git, svn, etc. for changes when they don't exist.
	} else if (isnan(p_value)) {
		return "nan";
	} else if (isinf(p_value)) {
		if (p_value > 0) {
			return "inf";
		} else {
			return "inf_neg";
		}
	} else {
		return rtoss(p_value);
	}
}

Error VariantWriter::write(const Variant &p_variant, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud, int p_recursion_count) {
	switch (p_variant.get_type()) {
		case Variant::NIL: {
			p_store_string_func(p_store_string_ud, "null");
		} break;
		case Variant::BOOL: {
			p_store_string_func(p_store_string_ud, p_variant.operator bool() ? "true" : "false");
		} break;
		case Variant::INT: {
			p_store_string_func(p_store_string_ud, itos(p_variant.operator int64_t()));
		} break;
		case Variant::REAL: {
			String s = rtos_fix(p_variant.operator real_t());
			if (s != "inf" && s != "inf_neg" && s != "nan") {
				if (s.find(".") == -1 && s.find("e") == -1) {
					s += ".0";
				}
			}
			p_store_string_func(p_store_string_ud, s);
		} break;
		case Variant::STRING: {
			String str = p_variant;

			str = "\"" + str.c_escape_multiline() + "\"";
			p_store_string_func(p_store_string_ud, str);
		} break;
		case Variant::VECTOR2: {
			Vector2 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector2( " + rtos_fix(v.x) + ", " + rtos_fix(v.y) + " )");
		} break;
		case Variant::RECT2: {
			Rect2 aabb = p_variant;
			p_store_string_func(p_store_string_ud, "Rect2( " + rtos_fix(aabb.position.x) + ", " + rtos_fix(aabb.position.y) + ", " + rtos_fix(aabb.size.x) + ", " + rtos_fix(aabb.size.y) + " )");

		} break;
		case Variant::VECTOR3: {
			Vector3 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector3( " + rtos_fix(v.x) + ", " + rtos_fix(v.y) + ", " + rtos_fix(v.z) + " )");
		} break;
		case Variant::PLANE: {
			Plane p = p_variant;
			p_store_string_func(p_store_string_ud, "Plane( " + rtos_fix(p.normal.x) + ", " + rtos_fix(p.normal.y) + ", " + rtos_fix(p.normal.z) + ", " + rtos_fix(p.d) + " )");

		} break;
		case Variant::AABB: {
			AABB aabb = p_variant;
			p_store_string_func(p_store_string_ud, "AABB( " + rtos_fix(aabb.position.x) + ", " + rtos_fix(aabb.position.y) + ", " + rtos_fix(aabb.position.z) + ", " + rtos_fix(aabb.size.x) + ", " + rtos_fix(aabb.size.y) + ", " + rtos_fix(aabb.size.z) + " )");

		} break;
		case Variant::QUAT: {
			Quat quat = p_variant;
			p_store_string_func(p_store_string_ud, "Quat( " + rtos_fix(quat.x) + ", " + rtos_fix(quat.y) + ", " + rtos_fix(quat.z) + ", " + rtos_fix(quat.w) + " )");

		} break;
		case Variant::TRANSFORM2D: {
			String s = "Transform2D( ";
			Transform2D m3 = p_variant;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 2; j++) {
					if (i != 0 || j != 0) {
						s += ", ";
					}
					s += rtos_fix(m3.elements[i][j]);
				}
			}

			p_store_string_func(p_store_string_ud, s + " )");

		} break;
		case Variant::BASIS: {
			String s = "Basis( ";
			Basis m3 = p_variant;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					if (i != 0 || j != 0) {
						s += ", ";
					}
					s += rtos_fix(m3.elements[i][j]);
				}
			}

			p_store_string_func(p_store_string_ud, s + " )");

		} break;
		case Variant::TRANSFORM: {
			String s = "Transform( ";
			Transform t = p_variant;
			Basis &m3 = t.basis;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					if (i != 0 || j != 0) {
						s += ", ";
					}
					s += rtos_fix(m3.elements[i][j]);
				}
			}

			s = s + ", " + rtos_fix(t.origin.x) + ", " + rtos_fix(t.origin.y) + ", " + rtos_fix(t.origin.z);

			p_store_string_func(p_store_string_ud, s + " )");
		} break;

		// misc types
		case Variant::COLOR: {
			Color c = p_variant;
			p_store_string_func(p_store_string_ud, "Color( " + rtos_fix(c.r) + ", " + rtos_fix(c.g) + ", " + rtos_fix(c.b) + ", " + rtos_fix(c.a) + " )");

		} break;
		case Variant::NODE_PATH: {
			String str = p_variant;

			str = "NodePath(\"" + str.c_escape() + "\")";
			p_store_string_func(p_store_string_ud, str);

		} break;

		case Variant::OBJECT: {
			if (unlikely(p_recursion_count > MAX_RECURSION)) {
				ERR_PRINT("Max recursion reached");
				p_store_string_func(p_store_string_ud, "null");
				return OK;
			}
			p_recursion_count++;

			Object *obj = p_variant;

			if (!obj) {
				p_store_string_func(p_store_string_ud, "null");
				break; // don't save it
			}

			RES res = p_variant;
			if (res.is_valid()) {
				//is resource
				String res_text;

				//try external function
				if (p_encode_res_func) {
					res_text = p_encode_res_func(p_encode_res_ud, res);
				}

				//try path because it's a file
				if (res_text == String() && res->get_path().is_resource_file()) {
					//external resource
					String path = res->get_path();
					res_text = "Resource( \"" + path + "\")";
				}

				//could come up with some sort of text
				if (res_text != String()) {
					p_store_string_func(p_store_string_ud, res_text);
					break;
				}
			}

			//store as generic object

			p_store_string_func(p_store_string_ud, "Object(" + obj->get_class() + ",");

			List<PropertyInfo> props;
			obj->get_property_list(&props);
			bool first = true;
			for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
				if (E->get().usage & PROPERTY_USAGE_STORAGE || E->get().usage & PROPERTY_USAGE_SCRIPT_VARIABLE) {
					//must be serialized

					if (first) {
						first = false;
					} else {
						p_store_string_func(p_store_string_ud, ",");
					}

					p_store_string_func(p_store_string_ud, "\"" + E->get().name + "\":");
					write(obj->get(E->get().name), p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud, p_recursion_count);
				}
			}

			p_store_string_func(p_store_string_ud, ")\n");

		} break;

		case Variant::DICTIONARY: {
			if (unlikely(p_recursion_count > MAX_RECURSION)) {
				ERR_PRINT("Max recursion reached");
				p_store_string_func(p_store_string_ud, "{}");
				return OK;
			}
			p_recursion_count++;

			Dictionary dict = p_variant;

			List<Variant> keys;
			dict.get_key_list(&keys);
			keys.sort();

			p_store_string_func(p_store_string_ud, "{\n");
			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
				/*
				if (!_check_type(dict[E->get()]))
					continue;
				*/
				write(E->get(), p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud, p_recursion_count);
				p_store_string_func(p_store_string_ud, ": ");
				write(dict[E->get()], p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud, p_recursion_count);
				if (E->next()) {
					p_store_string_func(p_store_string_ud, ",\n");
				} else {
					p_store_string_func(p_store_string_ud, "\n");
				}
			}

			p_store_string_func(p_store_string_ud, "}");

		} break;
		case Variant::ARRAY: {
			if (unlikely(p_recursion_count > MAX_RECURSION)) {
				ERR_PRINT("Max recursion reached");
				p_store_string_func(p_store_string_ud, "[]");
				return OK;
			}
			p_recursion_count++;

			p_store_string_func(p_store_string_ud, "[ ");
			Array array = p_variant;
			int len = array.size();
			for (int i = 0; i < len; i++) {
				if (i > 0) {
					p_store_string_func(p_store_string_ud, ", ");
				}
				write(array[i], p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud, p_recursion_count);
			}
			p_store_string_func(p_store_string_ud, " ]");

		} break;

		case Variant::POOL_BYTE_ARRAY: {
			p_store_string_func(p_store_string_ud, "PoolByteArray( ");
			String s;
			PoolVector<uint8_t> data = p_variant;
			int len = data.size();
			PoolVector<uint8_t>::Read r = data.read();
			const uint8_t *ptr = r.ptr();
			for (int i = 0; i < len; i++) {
				if (i > 0) {
					p_store_string_func(p_store_string_ud, ", ");
				}

				p_store_string_func(p_store_string_ud, itos(ptr[i]));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case Variant::POOL_INT_ARRAY: {
			p_store_string_func(p_store_string_ud, "PoolIntArray( ");
			PoolVector<int> data = p_variant;
			int len = data.size();
			PoolVector<int>::Read r = data.read();
			const int *ptr = r.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0) {
					p_store_string_func(p_store_string_ud, ", ");
				}

				p_store_string_func(p_store_string_ud, itos(ptr[i]));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case Variant::POOL_REAL_ARRAY: {
			p_store_string_func(p_store_string_ud, "PoolRealArray( ");
			PoolVector<real_t> data = p_variant;
			int len = data.size();
			PoolVector<real_t>::Read r = data.read();
			const real_t *ptr = r.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0) {
					p_store_string_func(p_store_string_ud, ", ");
				}
				p_store_string_func(p_store_string_ud, rtos_fix(ptr[i]));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case Variant::POOL_STRING_ARRAY: {
			p_store_string_func(p_store_string_ud, "PoolStringArray( ");
			PoolVector<String> data = p_variant;
			int len = data.size();
			PoolVector<String>::Read r = data.read();
			const String *ptr = r.ptr();
			String s;
			//write_string("\n");

			for (int i = 0; i < len; i++) {
				if (i > 0) {
					p_store_string_func(p_store_string_ud, ", ");
				}
				String str = ptr[i];
				p_store_string_func(p_store_string_ud, "\"" + str.c_escape() + "\"");
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case Variant::POOL_VECTOR2_ARRAY: {
			p_store_string_func(p_store_string_ud, "PoolVector2Array( ");
			PoolVector<Vector2> data = p_variant;
			int len = data.size();
			PoolVector<Vector2>::Read r = data.read();
			const Vector2 *ptr = r.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0) {
					p_store_string_func(p_store_string_ud, ", ");
				}
				p_store_string_func(p_store_string_ud, rtos_fix(ptr[i].x) + ", " + rtos_fix(ptr[i].y));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case Variant::POOL_VECTOR3_ARRAY: {
			p_store_string_func(p_store_string_ud, "PoolVector3Array( ");
			PoolVector<Vector3> data = p_variant;
			int len = data.size();
			PoolVector<Vector3>::Read r = data.read();
			const Vector3 *ptr = r.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0) {
					p_store_string_func(p_store_string_ud, ", ");
				}
				p_store_string_func(p_store_string_ud, rtos_fix(ptr[i].x) + ", " + rtos_fix(ptr[i].y) + ", " + rtos_fix(ptr[i].z));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case Variant::POOL_COLOR_ARRAY: {
			p_store_string_func(p_store_string_ud, "PoolColorArray( ");

			PoolVector<Color> data = p_variant;
			int len = data.size();
			PoolVector<Color>::Read r = data.read();
			const Color *ptr = r.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0) {
					p_store_string_func(p_store_string_ud, ", ");
				}

				p_store_string_func(p_store_string_ud, rtos_fix(ptr[i].r) + ", " + rtos_fix(ptr[i].g) + ", " + rtos_fix(ptr[i].b) + ", " + rtos_fix(ptr[i].a));
			}
			p_store_string_func(p_store_string_ud, " )");

		} break;

		default: {
		}
	}

	return OK;
}

static Error _write_to_str(void *ud, const String &p_string) {
	String *str = (String *)ud;
	(*str) += p_string;
	return OK;
}

Error VariantWriter::write_to_string(const Variant &p_variant, String &r_string, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {
	r_string = String();

	return write(p_variant, _write_to_str, &r_string, p_encode_res_func, p_encode_res_ud);
}
