/**************************************************************************/
/*  json.cpp                                                              */
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

#include "json.h"

#include "core/config/engine.h"
#include "core/object/script_language.h"
#include "core/variant/container_type_validate.h"

const char *JSON::tk_name[TK_MAX] = {
	"'{'",
	"'}'",
	"'['",
	"']'",
	"identifier",
	"string",
	"number",
	"':'",
	"','",
	"EOF",
};

String JSON::_make_indent(const String &p_indent, int p_size) {
	return p_indent.repeat(p_size);
}

String JSON::_stringify(const Variant &p_var, const String &p_indent, int p_cur_indent, bool p_sort_keys, HashSet<const void *> &p_markers, bool p_full_precision) {
	ERR_FAIL_COND_V_MSG(p_cur_indent > Variant::MAX_RECURSION_DEPTH, "...", "JSON structure is too deep. Bailing.");

	String colon = ":";
	String end_statement = "";

	if (!p_indent.is_empty()) {
		colon += " ";
		end_statement += "\n";
	}

	switch (p_var.get_type()) {
		case Variant::NIL:
			return "null";
		case Variant::BOOL:
			return p_var.operator bool() ? "true" : "false";
		case Variant::INT:
			return itos(p_var);
		case Variant::FLOAT: {
			double num = p_var;

			// Only for exactly 0. If we have approximately 0 let the user decide how much
			// precision they want.
			if (num == double(0)) {
				return String("0.0");
			}

			double magnitude = log10(Math::abs(num));
			int total_digits = p_full_precision ? 17 : 14;
			int precision = MAX(1, total_digits - (int)Math::floor(magnitude));

			return String::num(num, precision);
		}
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY:
		case Variant::ARRAY: {
			Array a = p_var;
			if (a.is_empty()) {
				return "[]";
			}
			String s = "[";
			s += end_statement;

			ERR_FAIL_COND_V_MSG(p_markers.has(a.id()), "\"[...]\"", "Converting circular structure to JSON.");
			p_markers.insert(a.id());

			bool first = true;
			for (const Variant &var : a) {
				if (first) {
					first = false;
				} else {
					s += ",";
					s += end_statement;
				}
				s += _make_indent(p_indent, p_cur_indent + 1) + _stringify(var, p_indent, p_cur_indent + 1, p_sort_keys, p_markers);
			}
			s += end_statement + _make_indent(p_indent, p_cur_indent) + "]";
			p_markers.erase(a.id());
			return s;
		}
		case Variant::DICTIONARY: {
			String s = "{";
			s += end_statement;
			Dictionary d = p_var;

			ERR_FAIL_COND_V_MSG(p_markers.has(d.id()), "\"{...}\"", "Converting circular structure to JSON.");
			p_markers.insert(d.id());

			List<Variant> keys;
			d.get_key_list(&keys);

			if (p_sort_keys) {
				keys.sort_custom<StringLikeVariantOrder>();
			}

			bool first_key = true;
			for (const Variant &E : keys) {
				if (first_key) {
					first_key = false;
				} else {
					s += ",";
					s += end_statement;
				}
				s += _make_indent(p_indent, p_cur_indent + 1) + _stringify(String(E), p_indent, p_cur_indent + 1, p_sort_keys, p_markers);
				s += colon;
				s += _stringify(d[E], p_indent, p_cur_indent + 1, p_sort_keys, p_markers);
			}

			s += end_statement + _make_indent(p_indent, p_cur_indent) + "}";
			p_markers.erase(d.id());
			return s;
		}
		default:
			return "\"" + String(p_var).json_escape() + "\"";
	}
}

Error JSON::_get_token(const char32_t *p_str, int &index, int p_len, Token &r_token, int &line, String &r_err_str) {
	while (p_len > 0) {
		switch (p_str[index]) {
			case '\n': {
				line++;
				index++;
				break;
			}
			case 0: {
				r_token.type = TK_EOF;
				return OK;
			} break;
			case '{': {
				r_token.type = TK_CURLY_BRACKET_OPEN;
				index++;
				return OK;
			}
			case '}': {
				r_token.type = TK_CURLY_BRACKET_CLOSE;
				index++;
				return OK;
			}
			case '[': {
				r_token.type = TK_BRACKET_OPEN;
				index++;
				return OK;
			}
			case ']': {
				r_token.type = TK_BRACKET_CLOSE;
				index++;
				return OK;
			}
			case ':': {
				r_token.type = TK_COLON;
				index++;
				return OK;
			}
			case ',': {
				r_token.type = TK_COMMA;
				index++;
				return OK;
			}
			case '"': {
				index++;
				String str;
				while (true) {
					if (p_str[index] == 0) {
						r_err_str = "Unterminated String";
						return ERR_PARSE_ERROR;
					} else if (p_str[index] == '"') {
						index++;
						break;
					} else if (p_str[index] == '\\') {
						//escaped characters...
						index++;
						char32_t next = p_str[index];
						if (next == 0) {
							r_err_str = "Unterminated String";
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
							case 'u': {
								// hex number
								for (int j = 0; j < 4; j++) {
									char32_t c = p_str[index + j + 1];
									if (c == 0) {
										r_err_str = "Unterminated String";
										return ERR_PARSE_ERROR;
									}
									if (!is_hex_digit(c)) {
										r_err_str = "Malformed hex constant in string";
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
								index += 4; //will add at the end anyway

								if ((res & 0xfffffc00) == 0xd800) {
									if (p_str[index + 1] != '\\' || p_str[index + 2] != 'u') {
										r_err_str = "Invalid UTF-16 sequence in string, unpaired lead surrogate";
										return ERR_PARSE_ERROR;
									}
									index += 2;
									char32_t trail = 0;
									for (int j = 0; j < 4; j++) {
										char32_t c = p_str[index + j + 1];
										if (c == 0) {
											r_err_str = "Unterminated String";
											return ERR_PARSE_ERROR;
										}
										if (!is_hex_digit(c)) {
											r_err_str = "Malformed hex constant in string";
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

										trail <<= 4;
										trail |= v;
									}
									if ((trail & 0xfffffc00) == 0xdc00) {
										res = (res << 10UL) + trail - ((0xd800 << 10UL) + 0xdc00 - 0x10000);
										index += 4; //will add at the end anyway
									} else {
										r_err_str = "Invalid UTF-16 sequence in string, unpaired lead surrogate";
										return ERR_PARSE_ERROR;
									}
								} else if ((res & 0xfffffc00) == 0xdc00) {
									r_err_str = "Invalid UTF-16 sequence in string, unpaired trail surrogate";
									return ERR_PARSE_ERROR;
								}

							} break;
							case '"':
							case '\\':
							case '/': {
								res = next;
							} break;
							default: {
								r_err_str = "Invalid escape sequence.";
								return ERR_PARSE_ERROR;
							}
						}

						str += res;

					} else {
						if (p_str[index] == '\n') {
							line++;
						}
						str += p_str[index];
					}
					index++;
				}

				r_token.type = TK_STRING;
				r_token.value = str;
				return OK;

			} break;
			default: {
				if (p_str[index] <= 32) {
					index++;
					break;
				}

				if (p_str[index] == '-' || is_digit(p_str[index])) {
					//a number
					const char32_t *rptr;
					double number = String::to_float(&p_str[index], &rptr);
					index += (rptr - &p_str[index]);
					r_token.type = TK_NUMBER;
					r_token.value = number;
					return OK;

				} else if (is_ascii_alphabet_char(p_str[index])) {
					String id;

					while (is_ascii_alphabet_char(p_str[index])) {
						id += p_str[index];
						index++;
					}

					r_token.type = TK_IDENTIFIER;
					r_token.value = id;
					return OK;
				} else {
					r_err_str = "Unexpected character.";
					return ERR_PARSE_ERROR;
				}
			}
		}
	}

	return ERR_PARSE_ERROR;
}

Error JSON::_parse_value(Variant &value, Token &token, const char32_t *p_str, int &index, int p_len, int &line, int p_depth, String &r_err_str) {
	if (p_depth > Variant::MAX_RECURSION_DEPTH) {
		r_err_str = "JSON structure is too deep. Bailing.";
		return ERR_OUT_OF_MEMORY;
	}

	if (token.type == TK_CURLY_BRACKET_OPEN) {
		Dictionary d;
		Error err = _parse_object(d, p_str, index, p_len, line, p_depth + 1, r_err_str);
		if (err) {
			return err;
		}
		value = d;
	} else if (token.type == TK_BRACKET_OPEN) {
		Array a;
		Error err = _parse_array(a, p_str, index, p_len, line, p_depth + 1, r_err_str);
		if (err) {
			return err;
		}
		value = a;
	} else if (token.type == TK_IDENTIFIER) {
		String id = token.value;
		if (id == "true") {
			value = true;
		} else if (id == "false") {
			value = false;
		} else if (id == "null") {
			value = Variant();
		} else {
			r_err_str = "Expected 'true','false' or 'null', got '" + id + "'.";
			return ERR_PARSE_ERROR;
		}
	} else if (token.type == TK_NUMBER) {
		value = token.value;
	} else if (token.type == TK_STRING) {
		value = token.value;
	} else {
		r_err_str = "Expected value, got " + String(tk_name[token.type]) + ".";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error JSON::_parse_array(Array &array, const char32_t *p_str, int &index, int p_len, int &line, int p_depth, String &r_err_str) {
	Token token;
	bool need_comma = false;

	while (index < p_len) {
		Error err = _get_token(p_str, index, p_len, token, line, r_err_str);
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
		err = _parse_value(v, token, p_str, index, p_len, line, p_depth, r_err_str);
		if (err) {
			return err;
		}

		array.push_back(v);
		need_comma = true;
	}

	r_err_str = "Expected ']'";
	return ERR_PARSE_ERROR;
}

Error JSON::_parse_object(Dictionary &object, const char32_t *p_str, int &index, int p_len, int &line, int p_depth, String &r_err_str) {
	bool at_key = true;
	String key;
	Token token;
	bool need_comma = false;

	while (index < p_len) {
		if (at_key) {
			Error err = _get_token(p_str, index, p_len, token, line, r_err_str);
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

			if (token.type != TK_STRING) {
				r_err_str = "Expected key";
				return ERR_PARSE_ERROR;
			}

			key = token.value;
			err = _get_token(p_str, index, p_len, token, line, r_err_str);
			if (err != OK) {
				return err;
			}
			if (token.type != TK_COLON) {
				r_err_str = "Expected ':'";
				return ERR_PARSE_ERROR;
			}
			at_key = false;
		} else {
			Error err = _get_token(p_str, index, p_len, token, line, r_err_str);
			if (err != OK) {
				return err;
			}

			Variant v;
			err = _parse_value(v, token, p_str, index, p_len, line, p_depth, r_err_str);
			if (err) {
				return err;
			}
			object[key] = v;
			need_comma = true;
			at_key = true;
		}
	}

	r_err_str = "Expected '}'";
	return ERR_PARSE_ERROR;
}

void JSON::set_data(const Variant &p_data) {
	data = p_data;
	text.clear();
}

Error JSON::_parse_string(const String &p_json, Variant &r_ret, String &r_err_str, int &r_err_line) {
	const char32_t *str = p_json.ptr();
	int idx = 0;
	int len = p_json.length();
	Token token;
	r_err_line = 0;
	String aux_key;

	Error err = _get_token(str, idx, len, token, r_err_line, r_err_str);
	if (err) {
		return err;
	}

	err = _parse_value(r_ret, token, str, idx, len, r_err_line, 0, r_err_str);

	// Check if EOF is reached
	// or it's a type of the next token.
	if (err == OK && idx < len) {
		err = _get_token(str, idx, len, token, r_err_line, r_err_str);

		if (err || token.type != TK_EOF) {
			r_err_str = "Expected 'EOF'";
			// Reset return value to empty `Variant`
			r_ret = Variant();
			return ERR_PARSE_ERROR;
		}
	}

	return err;
}

Error JSON::parse(const String &p_json_string, bool p_keep_text) {
	Error err = _parse_string(p_json_string, data, err_str, err_line);
	if (err == Error::OK) {
		err_line = 0;
	}
	if (p_keep_text) {
		text = p_json_string;
	}
	return err;
}

String JSON::get_parsed_text() const {
	return text;
}

String JSON::stringify(const Variant &p_var, const String &p_indent, bool p_sort_keys, bool p_full_precision) {
	Ref<JSON> json;
	json.instantiate();
	HashSet<const void *> markers;
	return json->_stringify(p_var, p_indent, 0, p_sort_keys, markers, p_full_precision);
}

Variant JSON::parse_string(const String &p_json_string) {
	Ref<JSON> json;
	json.instantiate();
	Error error = json->parse(p_json_string);
	ERR_FAIL_COND_V_MSG(error != Error::OK, Variant(), vformat("Parse JSON failed. Error at line %d: %s", json->get_error_line(), json->get_error_message()));
	return json->get_data();
}

void JSON::_bind_methods() {
	ClassDB::bind_static_method("JSON", D_METHOD("stringify", "data", "indent", "sort_keys", "full_precision"), &JSON::stringify, DEFVAL(""), DEFVAL(true), DEFVAL(false));
	ClassDB::bind_static_method("JSON", D_METHOD("parse_string", "json_string"), &JSON::parse_string);
	ClassDB::bind_method(D_METHOD("parse", "json_text", "keep_text"), &JSON::parse, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("get_data"), &JSON::get_data);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &JSON::set_data);
	ClassDB::bind_method(D_METHOD("get_parsed_text"), &JSON::get_parsed_text);
	ClassDB::bind_method(D_METHOD("get_error_line"), &JSON::get_error_line);
	ClassDB::bind_method(D_METHOD("get_error_message"), &JSON::get_error_message);

	ClassDB::bind_static_method("JSON", D_METHOD("from_native", "variant", "full_objects"), &JSON::from_native, DEFVAL(false));
	ClassDB::bind_static_method("JSON", D_METHOD("to_native", "json", "allow_objects"), &JSON::to_native, DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::NIL, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), "set_data", "get_data"); // Ensures that it can be serialized as binary.
}

#define TYPE "type"
#define ELEM_TYPE "elem_type"
#define KEY_TYPE "key_type"
#define VALUE_TYPE "value_type"
#define ARGS "args"
#define PROPS "props"

static bool _encode_container_type(Dictionary &r_dict, const String &p_key, const ContainerType &p_type, bool p_full_objects) {
	if (p_type.builtin_type != Variant::NIL) {
		if (p_type.script.is_valid()) {
			ERR_FAIL_COND_V(!p_full_objects, false);
			const String path = p_type.script->get_path();
			ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://"), false, "Failed to encode a path to a custom script for a container type.");
			r_dict[p_key] = path;
		} else if (p_type.class_name != StringName()) {
			ERR_FAIL_COND_V(!p_full_objects, false);
			r_dict[p_key] = String(p_type.class_name);
		} else {
			// No need to check `p_full_objects` since `class_name` should be non-empty for `builtin_type == Variant::OBJECT`.
			r_dict[p_key] = Variant::get_type_name(p_type.builtin_type);
		}
	}
	return true;
}

Variant JSON::_from_native(const Variant &p_variant, bool p_full_objects, int p_depth) {
#define RETURN_ARGS                                           \
	Dictionary ret;                                           \
	ret[TYPE] = Variant::get_type_name(p_variant.get_type()); \
	ret[ARGS] = args;                                         \
	return ret

	switch (p_variant.get_type()) {
		case Variant::NIL:
		case Variant::BOOL: {
			return p_variant;
		} break;

		case Variant::INT: {
			return "i:" + String(p_variant);
		} break;
		case Variant::FLOAT: {
			return "f:" + String(p_variant);
		} break;
		case Variant::STRING: {
			return "s:" + String(p_variant);
		} break;
		case Variant::STRING_NAME: {
			return "sn:" + String(p_variant);
		} break;
		case Variant::NODE_PATH: {
			return "np:" + String(p_variant);
		} break;

		case Variant::RID:
		case Variant::CALLABLE:
		case Variant::SIGNAL: {
			Dictionary ret;
			ret[TYPE] = Variant::get_type_name(p_variant.get_type());
			return ret;
		} break;

		case Variant::VECTOR2: {
			const Vector2 v = p_variant;

			Array args;
			args.push_back(v.x);
			args.push_back(v.y);

			RETURN_ARGS;
		} break;
		case Variant::VECTOR2I: {
			const Vector2i v = p_variant;

			Array args;
			args.push_back(v.x);
			args.push_back(v.y);

			RETURN_ARGS;
		} break;
		case Variant::RECT2: {
			const Rect2 r = p_variant;

			Array args;
			args.push_back(r.position.x);
			args.push_back(r.position.y);
			args.push_back(r.size.width);
			args.push_back(r.size.height);

			RETURN_ARGS;
		} break;
		case Variant::RECT2I: {
			const Rect2i r = p_variant;

			Array args;
			args.push_back(r.position.x);
			args.push_back(r.position.y);
			args.push_back(r.size.width);
			args.push_back(r.size.height);

			RETURN_ARGS;
		} break;
		case Variant::VECTOR3: {
			const Vector3 v = p_variant;

			Array args;
			args.push_back(v.x);
			args.push_back(v.y);
			args.push_back(v.z);

			RETURN_ARGS;
		} break;
		case Variant::VECTOR3I: {
			const Vector3i v = p_variant;

			Array args;
			args.push_back(v.x);
			args.push_back(v.y);
			args.push_back(v.z);

			RETURN_ARGS;
		} break;
		case Variant::TRANSFORM2D: {
			const Transform2D t = p_variant;

			Array args;
			args.push_back(t[0].x);
			args.push_back(t[0].y);
			args.push_back(t[1].x);
			args.push_back(t[1].y);
			args.push_back(t[2].x);
			args.push_back(t[2].y);

			RETURN_ARGS;
		} break;
		case Variant::VECTOR4: {
			const Vector4 v = p_variant;

			Array args;
			args.push_back(v.x);
			args.push_back(v.y);
			args.push_back(v.z);
			args.push_back(v.w);

			RETURN_ARGS;
		} break;
		case Variant::VECTOR4I: {
			const Vector4i v = p_variant;

			Array args;
			args.push_back(v.x);
			args.push_back(v.y);
			args.push_back(v.z);
			args.push_back(v.w);

			RETURN_ARGS;
		} break;
		case Variant::PLANE: {
			const Plane p = p_variant;

			Array args;
			args.push_back(p.normal.x);
			args.push_back(p.normal.y);
			args.push_back(p.normal.z);
			args.push_back(p.d);

			RETURN_ARGS;
		} break;
		case Variant::QUATERNION: {
			const Quaternion q = p_variant;

			Array args;
			args.push_back(q.x);
			args.push_back(q.y);
			args.push_back(q.z);
			args.push_back(q.w);

			RETURN_ARGS;
		} break;
		case Variant::AABB: {
			const AABB aabb = p_variant;

			Array args;
			args.push_back(aabb.position.x);
			args.push_back(aabb.position.y);
			args.push_back(aabb.position.z);
			args.push_back(aabb.size.x);
			args.push_back(aabb.size.y);
			args.push_back(aabb.size.z);

			RETURN_ARGS;
		} break;
		case Variant::BASIS: {
			const Basis b = p_variant;

			Array args;
			args.push_back(b.get_column(0).x);
			args.push_back(b.get_column(0).y);
			args.push_back(b.get_column(0).z);
			args.push_back(b.get_column(1).x);
			args.push_back(b.get_column(1).y);
			args.push_back(b.get_column(1).z);
			args.push_back(b.get_column(2).x);
			args.push_back(b.get_column(2).y);
			args.push_back(b.get_column(2).z);

			RETURN_ARGS;
		} break;
		case Variant::TRANSFORM3D: {
			const Transform3D t = p_variant;

			Array args;
			args.push_back(t.basis.get_column(0).x);
			args.push_back(t.basis.get_column(0).y);
			args.push_back(t.basis.get_column(0).z);
			args.push_back(t.basis.get_column(1).x);
			args.push_back(t.basis.get_column(1).y);
			args.push_back(t.basis.get_column(1).z);
			args.push_back(t.basis.get_column(2).x);
			args.push_back(t.basis.get_column(2).y);
			args.push_back(t.basis.get_column(2).z);
			args.push_back(t.origin.x);
			args.push_back(t.origin.y);
			args.push_back(t.origin.z);

			RETURN_ARGS;
		} break;
		case Variant::PROJECTION: {
			const Projection p = p_variant;

			Array args;
			args.push_back(p[0].x);
			args.push_back(p[0].y);
			args.push_back(p[0].z);
			args.push_back(p[0].w);
			args.push_back(p[1].x);
			args.push_back(p[1].y);
			args.push_back(p[1].z);
			args.push_back(p[1].w);
			args.push_back(p[2].x);
			args.push_back(p[2].y);
			args.push_back(p[2].z);
			args.push_back(p[2].w);
			args.push_back(p[3].x);
			args.push_back(p[3].y);
			args.push_back(p[3].z);
			args.push_back(p[3].w);

			RETURN_ARGS;
		} break;
		case Variant::COLOR: {
			const Color c = p_variant;

			Array args;
			args.push_back(c.r);
			args.push_back(c.g);
			args.push_back(c.b);
			args.push_back(c.a);

			RETURN_ARGS;
		} break;

		case Variant::OBJECT: {
			ERR_FAIL_COND_V(!p_full_objects, Variant());

			ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, Variant(), "Variant is too deep. Bailing.");

			const Object *obj = p_variant.get_validated_object();
			if (obj == nullptr) {
				return Variant();
			}

			ERR_FAIL_COND_V(!ClassDB::can_instantiate(obj->get_class()), Variant());

			List<PropertyInfo> prop_list;
			obj->get_property_list(&prop_list);

			Array props;
			for (const PropertyInfo &pi : prop_list) {
				if (!(pi.usage & PROPERTY_USAGE_STORAGE)) {
					continue;
				}

				Variant value;
				if (pi.name == CoreStringName(script)) {
					const Ref<Script> script = obj->get_script();
					if (script.is_valid()) {
						const String path = script->get_path();
						ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://"), Variant(), "Failed to encode a path to a custom script.");
						value = path;
					}
				} else {
					value = obj->get(pi.name);
				}

				props.push_back(pi.name);
				props.push_back(_from_native(value, p_full_objects, p_depth + 1));
			}

			Dictionary ret;
			ret[TYPE] = obj->get_class();
			ret[PROPS] = props;
			return ret;
		} break;

		case Variant::DICTIONARY: {
			const Dictionary dict = p_variant;

			Array args;

			Dictionary ret;
			ret[TYPE] = Variant::get_type_name(p_variant.get_type());
			if (!_encode_container_type(ret, KEY_TYPE, dict.get_key_type(), p_full_objects)) {
				return Variant();
			}
			if (!_encode_container_type(ret, VALUE_TYPE, dict.get_value_type(), p_full_objects)) {
				return Variant();
			}
			ret[ARGS] = args;

			ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, ret, "Variant is too deep. Bailing.");

			List<Variant> keys;
			dict.get_key_list(&keys);

			for (const Variant &key : keys) {
				args.push_back(_from_native(key, p_full_objects, p_depth + 1));
				args.push_back(_from_native(dict[key], p_full_objects, p_depth + 1));
			}

			return ret;
		} break;

		case Variant::ARRAY: {
			const Array arr = p_variant;

			Variant ret;
			Array args;

			if (arr.is_typed()) {
				Dictionary d;
				d[TYPE] = Variant::get_type_name(p_variant.get_type());
				if (!_encode_container_type(d, ELEM_TYPE, arr.get_element_type(), p_full_objects)) {
					return Variant();
				}
				d[ARGS] = args;
				ret = d;
			} else {
				ret = args;
			}

			ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, ret, "Variant is too deep. Bailing.");

			for (int i = 0; i < arr.size(); i++) {
				args.push_back(_from_native(arr[i], p_full_objects, p_depth + 1));
			}

			return ret;
		} break;

		case Variant::PACKED_BYTE_ARRAY: {
			const PackedByteArray arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				args.push_back(arr[i]);
			}

			RETURN_ARGS;
		} break;
		case Variant::PACKED_INT32_ARRAY: {
			const PackedInt32Array arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				args.push_back(arr[i]);
			}

			RETURN_ARGS;
		} break;
		case Variant::PACKED_INT64_ARRAY: {
			const PackedInt64Array arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				args.push_back(arr[i]);
			}

			RETURN_ARGS;
		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			const PackedFloat32Array arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				args.push_back(arr[i]);
			}

			RETURN_ARGS;
		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			const PackedFloat64Array arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				args.push_back(arr[i]);
			}

			RETURN_ARGS;
		} break;
		case Variant::PACKED_STRING_ARRAY: {
			const PackedStringArray arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				args.push_back(arr[i]);
			}

			RETURN_ARGS;
		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			const PackedVector2Array arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				Vector2 v = arr[i];
				args.push_back(v.x);
				args.push_back(v.y);
			}

			RETURN_ARGS;
		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			const PackedVector3Array arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				Vector3 v = arr[i];
				args.push_back(v.x);
				args.push_back(v.y);
				args.push_back(v.z);
			}

			RETURN_ARGS;
		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			const PackedColorArray arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				Color v = arr[i];
				args.push_back(v.r);
				args.push_back(v.g);
				args.push_back(v.b);
				args.push_back(v.a);
			}

			RETURN_ARGS;
		} break;
		case Variant::PACKED_VECTOR4_ARRAY: {
			const PackedVector4Array arr = p_variant;

			Array args;
			for (int i = 0; i < arr.size(); i++) {
				Vector4 v = arr[i];
				args.push_back(v.x);
				args.push_back(v.y);
				args.push_back(v.z);
				args.push_back(v.w);
			}

			RETURN_ARGS;
		} break;

		case Variant::VARIANT_MAX: {
			// Nothing to do.
		} break;
	}

#undef RETURN_ARGS

	ERR_FAIL_V_MSG(Variant(), vformat(R"(Unhandled Variant type "%s".)", Variant::get_type_name(p_variant.get_type())));
}

static bool _decode_container_type(const Dictionary &p_dict, const String &p_key, ContainerType &r_type, bool p_allow_objects) {
	if (!p_dict.has(p_key)) {
		return true;
	}

	const String type_name = p_dict[p_key];

	const Variant::Type builtin_type = Variant::get_type_by_name(type_name);
	if (builtin_type < Variant::VARIANT_MAX && builtin_type != Variant::OBJECT) {
		r_type.builtin_type = builtin_type;
		return true;
	}

	if (ClassDB::class_exists(type_name)) {
		ERR_FAIL_COND_V(!p_allow_objects, false);

		r_type.builtin_type = Variant::OBJECT;
		r_type.class_name = type_name;
		return true;
	}

	if (type_name.begins_with("res://")) {
		ERR_FAIL_COND_V(!p_allow_objects, false);

		ERR_FAIL_COND_V_MSG(!ResourceLoader::exists(type_name, "Script"), false, vformat(R"(Invalid script path "%s".)", type_name));
		const Ref<Script> script = ResourceLoader::load(type_name, "Script");
		ERR_FAIL_COND_V_MSG(script.is_null(), false, vformat(R"(Can't load script at path "%s".)", type_name));

		r_type.builtin_type = Variant::OBJECT;
		r_type.class_name = script->get_instance_base_type();
		r_type.script = script;
		return true;
	}

	ERR_FAIL_V_MSG(false, vformat(R"(Invalid type "%s".)", type_name));
}

Variant JSON::_to_native(const Variant &p_json, bool p_allow_objects, int p_depth) {
	switch (p_json.get_type()) {
		case Variant::NIL:
		case Variant::BOOL: {
			return p_json;
		} break;

		case Variant::STRING: {
			const String s = p_json;

			if (s.begins_with("i:")) {
				return s.substr(2).to_int();
			} else if (s.begins_with("f:")) {
				return s.substr(2).to_float();
			} else if (s.begins_with("s:")) {
				return s.substr(2);
			} else if (s.begins_with("sn:")) {
				return StringName(s.substr(3));
			} else if (s.begins_with("np:")) {
				return NodePath(s.substr(3));
			}

			ERR_FAIL_V_MSG(Variant(), "Invalid string, the type prefix is not recognized.");
		} break;

		case Variant::DICTIONARY: {
			const Dictionary dict = p_json;

			ERR_FAIL_COND_V(!dict.has(TYPE), Variant());

#define LOAD_ARGS()                              \
	ERR_FAIL_COND_V(!dict.has(ARGS), Variant()); \
	const Array args = dict[ARGS]

#define LOAD_ARGS_CHECK_SIZE(m_size)             \
	ERR_FAIL_COND_V(!dict.has(ARGS), Variant()); \
	const Array args = dict[ARGS];               \
	ERR_FAIL_COND_V(args.size() != (m_size), Variant())

#define LOAD_ARGS_CHECK_FACTOR(m_factor)         \
	ERR_FAIL_COND_V(!dict.has(ARGS), Variant()); \
	const Array args = dict[ARGS];               \
	ERR_FAIL_COND_V(args.size() % (m_factor) != 0, Variant())

			switch (Variant::get_type_by_name(dict[TYPE])) {
				case Variant::NIL:
				case Variant::BOOL: {
					ERR_FAIL_V_MSG(Variant(), vformat(R"(Unexpected "%s": Variant type "%s" is JSON-compliant.)", TYPE, dict[TYPE]));
				} break;

				case Variant::INT:
				case Variant::FLOAT:
				case Variant::STRING:
				case Variant::STRING_NAME:
				case Variant::NODE_PATH: {
					ERR_FAIL_V_MSG(Variant(), vformat(R"(Unexpected "%s": Variant type "%s" must be represented as a string.)", TYPE, dict[TYPE]));
				} break;

				case Variant::RID: {
					return RID();
				} break;
				case Variant::CALLABLE: {
					return Callable();
				} break;
				case Variant::SIGNAL: {
					return Signal();
				} break;

				case Variant::VECTOR2: {
					LOAD_ARGS_CHECK_SIZE(2);

					Vector2 v;
					v.x = args[0];
					v.y = args[1];

					return v;
				} break;
				case Variant::VECTOR2I: {
					LOAD_ARGS_CHECK_SIZE(2);

					Vector2i v;
					v.x = args[0];
					v.y = args[1];

					return v;
				} break;
				case Variant::RECT2: {
					LOAD_ARGS_CHECK_SIZE(4);

					Rect2 r;
					r.position = Point2(args[0], args[1]);
					r.size = Size2(args[2], args[3]);

					return r;
				} break;
				case Variant::RECT2I: {
					LOAD_ARGS_CHECK_SIZE(4);

					Rect2i r;
					r.position = Point2i(args[0], args[1]);
					r.size = Size2i(args[2], args[3]);

					return r;
				} break;
				case Variant::VECTOR3: {
					LOAD_ARGS_CHECK_SIZE(3);

					Vector3 v;
					v.x = args[0];
					v.y = args[1];
					v.z = args[2];

					return v;
				} break;
				case Variant::VECTOR3I: {
					LOAD_ARGS_CHECK_SIZE(3);

					Vector3i v;
					v.x = args[0];
					v.y = args[1];
					v.z = args[2];

					return v;
				} break;
				case Variant::TRANSFORM2D: {
					LOAD_ARGS_CHECK_SIZE(6);

					Transform2D t;
					t[0] = Vector2(args[0], args[1]);
					t[1] = Vector2(args[2], args[3]);
					t[2] = Vector2(args[4], args[5]);

					return t;
				} break;
				case Variant::VECTOR4: {
					LOAD_ARGS_CHECK_SIZE(4);

					Vector4 v;
					v.x = args[0];
					v.y = args[1];
					v.z = args[2];
					v.w = args[3];

					return v;
				} break;
				case Variant::VECTOR4I: {
					LOAD_ARGS_CHECK_SIZE(4);

					Vector4i v;
					v.x = args[0];
					v.y = args[1];
					v.z = args[2];
					v.w = args[3];

					return v;
				} break;
				case Variant::PLANE: {
					LOAD_ARGS_CHECK_SIZE(4);

					Plane p;
					p.normal = Vector3(args[0], args[1], args[2]);
					p.d = args[3];

					return p;
				} break;
				case Variant::QUATERNION: {
					LOAD_ARGS_CHECK_SIZE(4);

					Quaternion q;
					q.x = args[0];
					q.y = args[1];
					q.z = args[2];
					q.w = args[3];

					return q;
				} break;
				case Variant::AABB: {
					LOAD_ARGS_CHECK_SIZE(6);

					AABB aabb;
					aabb.position = Vector3(args[0], args[1], args[2]);
					aabb.size = Vector3(args[3], args[4], args[5]);

					return aabb;
				} break;
				case Variant::BASIS: {
					LOAD_ARGS_CHECK_SIZE(9);

					Basis b;
					b.set_column(0, Vector3(args[0], args[1], args[2]));
					b.set_column(1, Vector3(args[3], args[4], args[5]));
					b.set_column(2, Vector3(args[6], args[7], args[8]));

					return b;
				} break;
				case Variant::TRANSFORM3D: {
					LOAD_ARGS_CHECK_SIZE(12);

					Transform3D t;
					t.basis.set_column(0, Vector3(args[0], args[1], args[2]));
					t.basis.set_column(1, Vector3(args[3], args[4], args[5]));
					t.basis.set_column(2, Vector3(args[6], args[7], args[8]));
					t.origin = Vector3(args[9], args[10], args[11]);

					return t;
				} break;
				case Variant::PROJECTION: {
					LOAD_ARGS_CHECK_SIZE(16);

					Projection p;
					p[0] = Vector4(args[0], args[1], args[2], args[3]);
					p[1] = Vector4(args[4], args[5], args[6], args[7]);
					p[2] = Vector4(args[8], args[9], args[10], args[11]);
					p[3] = Vector4(args[12], args[13], args[14], args[15]);

					return p;
				} break;
				case Variant::COLOR: {
					LOAD_ARGS_CHECK_SIZE(4);

					Color c;
					c.r = args[0];
					c.g = args[1];
					c.b = args[2];
					c.a = args[3];

					return c;
				} break;

				case Variant::OBJECT: {
					// Nothing to do at this stage. `Object` should be treated as a class, not as a built-in type.
				} break;

				case Variant::DICTIONARY: {
					LOAD_ARGS_CHECK_FACTOR(2);

					ContainerType key_type;
					if (!_decode_container_type(dict, KEY_TYPE, key_type, p_allow_objects)) {
						return Variant();
					}

					ContainerType value_type;
					if (!_decode_container_type(dict, VALUE_TYPE, value_type, p_allow_objects)) {
						return Variant();
					}

					Dictionary ret;

					if (key_type.builtin_type != Variant::NIL || value_type.builtin_type != Variant::NIL) {
						ret.set_typed(key_type, value_type);
					}

					ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, ret, "Variant is too deep. Bailing.");

					for (int i = 0; i < args.size() / 2; i++) {
						ret[_to_native(args[i * 2 + 0], p_allow_objects, p_depth + 1)] = _to_native(args[i * 2 + 1], p_allow_objects, p_depth + 1);
					}

					return ret;
				} break;

				case Variant::ARRAY: {
					LOAD_ARGS();

					ContainerType elem_type;
					if (!_decode_container_type(dict, ELEM_TYPE, elem_type, p_allow_objects)) {
						return Variant();
					}

					Array ret;

					if (elem_type.builtin_type != Variant::NIL) {
						ret.set_typed(elem_type);
					}

					ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, ret, "Variant is too deep. Bailing.");

					ret.resize(args.size());
					for (int i = 0; i < args.size(); i++) {
						ret[i] = _to_native(args[i], p_allow_objects, p_depth + 1);
					}

					return ret;
				} break;

				case Variant::PACKED_BYTE_ARRAY: {
					LOAD_ARGS();

					PackedByteArray arr;
					arr.resize(args.size());
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = args[i];
					}

					return arr;
				} break;
				case Variant::PACKED_INT32_ARRAY: {
					LOAD_ARGS();

					PackedInt32Array arr;
					arr.resize(args.size());
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = args[i];
					}

					return arr;
				} break;
				case Variant::PACKED_INT64_ARRAY: {
					LOAD_ARGS();

					PackedInt64Array arr;
					arr.resize(args.size());
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = args[i];
					}

					return arr;
				} break;
				case Variant::PACKED_FLOAT32_ARRAY: {
					LOAD_ARGS();

					PackedFloat32Array arr;
					arr.resize(args.size());
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = args[i];
					}

					return arr;
				} break;
				case Variant::PACKED_FLOAT64_ARRAY: {
					LOAD_ARGS();

					PackedFloat64Array arr;
					arr.resize(args.size());
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = args[i];
					}

					return arr;
				} break;
				case Variant::PACKED_STRING_ARRAY: {
					LOAD_ARGS();

					PackedStringArray arr;
					arr.resize(args.size());
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = args[i];
					}

					return arr;
				} break;
				case Variant::PACKED_VECTOR2_ARRAY: {
					LOAD_ARGS_CHECK_FACTOR(2);

					PackedVector2Array arr;
					arr.resize(args.size() / 2);
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = Vector2(args[i * 2 + 0], args[i * 2 + 1]);
					}

					return arr;
				} break;
				case Variant::PACKED_VECTOR3_ARRAY: {
					LOAD_ARGS_CHECK_FACTOR(3);

					PackedVector3Array arr;
					arr.resize(args.size() / 3);
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = Vector3(args[i * 3 + 0], args[i * 3 + 1], args[i * 3 + 2]);
					}

					return arr;
				} break;
				case Variant::PACKED_COLOR_ARRAY: {
					LOAD_ARGS_CHECK_FACTOR(4);

					PackedColorArray arr;
					arr.resize(args.size() / 4);
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = Color(args[i * 4 + 0], args[i * 4 + 1], args[i * 4 + 2], args[i * 4 + 3]);
					}

					return arr;
				} break;
				case Variant::PACKED_VECTOR4_ARRAY: {
					LOAD_ARGS_CHECK_FACTOR(4);

					PackedVector4Array arr;
					arr.resize(args.size() / 4);
					for (int i = 0; i < arr.size(); i++) {
						arr.write[i] = Vector4(args[i * 4 + 0], args[i * 4 + 1], args[i * 4 + 2], args[i * 4 + 3]);
					}

					return arr;
				} break;

				case Variant::VARIANT_MAX: {
					// Nothing to do.
				} break;
			}

#undef LOAD_ARGS
#undef LOAD_ARGS_CHECK_SIZE
#undef LOAD_ARGS_CHECK_FACTOR

			if (ClassDB::class_exists(dict[TYPE])) {
				ERR_FAIL_COND_V(!p_allow_objects, Variant());

				ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, Variant(), "Variant is too deep. Bailing.");

				ERR_FAIL_COND_V(!dict.has(PROPS), Variant());
				const Array props = dict[PROPS];
				ERR_FAIL_COND_V(props.size() % 2 != 0, Variant());

				ERR_FAIL_COND_V(!ClassDB::can_instantiate(dict[TYPE]), Variant());

				Object *obj = ClassDB::instantiate(dict[TYPE]);
				ERR_FAIL_NULL_V(obj, Variant());

				// Avoid premature free `RefCounted`. This must be done before properties are initialized,
				// since script functions (setters, implicit initializer) may be called. See GH-68666.
				Variant variant;
				if (Object::cast_to<RefCounted>(obj)) {
					const Ref<RefCounted> ref = Ref<RefCounted>(Object::cast_to<RefCounted>(obj));
					variant = ref;
				} else {
					variant = obj;
				}

				for (int i = 0; i < props.size() / 2; i++) {
					const StringName name = props[i * 2 + 0];
					const Variant value = _to_native(props[i * 2 + 1], p_allow_objects, p_depth + 1);

					if (name == CoreStringName(script) && value.get_type() != Variant::NIL) {
						const String path = value;
						ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://") || !ResourceLoader::exists(path, "Script"),
								Variant(),
								vformat(R"(Invalid script path "%s".)", path));

						const Ref<Script> script = ResourceLoader::load(path, "Script");
						ERR_FAIL_COND_V_MSG(script.is_null(), Variant(), vformat(R"(Can't load script at path "%s".)", path));

						obj->set_script(script);
					} else {
						obj->set(name, value);
					}
				}

				return variant;
			}

			ERR_FAIL_V_MSG(Variant(), vformat(R"(Invalid type "%s".)", dict[TYPE]));
		} break;

		case Variant::ARRAY: {
			ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, Array(), "Variant is too deep. Bailing.");

			const Array arr = p_json;

			Array ret;
			ret.resize(arr.size());
			for (int i = 0; i < arr.size(); i++) {
				ret[i] = _to_native(arr[i], p_allow_objects, p_depth + 1);
			}

			return ret;
		} break;

		default: {
			// Nothing to do.
		} break;
	}

	ERR_FAIL_V_MSG(Variant(), vformat(R"(Variant type "%s" is not JSON-compliant.)", Variant::get_type_name(p_json.get_type())));
}

#undef TYPE
#undef ELEM_TYPE
#undef KEY_TYPE
#undef VALUE_TYPE
#undef ARGS
#undef PROPS

////////////

Ref<Resource> ResourceFormatLoaderJSON::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	if (!FileAccess::exists(p_path)) {
		*r_error = ERR_FILE_NOT_FOUND;
		return Ref<Resource>();
	}

	Ref<JSON> json;
	json.instantiate();

	Error err = json->parse(FileAccess::get_file_as_string(p_path), Engine::get_singleton()->is_editor_hint());
	if (err != OK) {
		String err_text = "Error parsing JSON file at '" + p_path + "', on line " + itos(json->get_error_line()) + ": " + json->get_error_message();

		if (Engine::get_singleton()->is_editor_hint()) {
			// If running on editor, still allow opening the JSON so the code editor can edit it.
			WARN_PRINT(err_text);
		} else {
			if (r_error) {
				*r_error = err;
			}
			ERR_PRINT(err_text);
			return Ref<Resource>();
		}
	}

	if (r_error) {
		*r_error = OK;
	}

	return json;
}

void ResourceFormatLoaderJSON::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("json");
}

bool ResourceFormatLoaderJSON::handles_type(const String &p_type) const {
	return (p_type == "JSON");
}

String ResourceFormatLoaderJSON::get_resource_type(const String &p_path) const {
	String el = p_path.get_extension().to_lower();
	if (el == "json") {
		return "JSON";
	}
	return "";
}

Error ResourceFormatSaverJSON::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<JSON> json = p_resource;
	ERR_FAIL_COND_V(json.is_null(), ERR_INVALID_PARAMETER);

	String source = json->get_parsed_text().is_empty() ? JSON::stringify(json->get_data(), "\t", false, true) : json->get_parsed_text();

	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);

	ERR_FAIL_COND_V_MSG(err, err, vformat("Cannot save json '%s'.", p_path));

	file->store_string(source);
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

void ResourceFormatSaverJSON::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	Ref<JSON> json = p_resource;
	if (json.is_valid()) {
		p_extensions->push_back("json");
	}
}

bool ResourceFormatSaverJSON::recognize(const Ref<Resource> &p_resource) const {
	return p_resource->get_class_name() == "JSON"; //only json, not inherited
}
