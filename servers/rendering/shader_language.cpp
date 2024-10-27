/**************************************************************************/
/*  shader_language.cpp                                                   */
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

#include "shader_language.h"

#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/templates/local_vector.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering_server.h"
#include "shader_types.h"

#define HAS_WARNING(flag) (warning_flags & flag)

int ShaderLanguage::instance_counter = 0;

String ShaderLanguage::get_operator_text(Operator p_op) {
	static const char *op_names[OP_MAX] = { "==",
		"!=",
		"<",
		"<=",
		">",
		">=",
		"&&",
		"||",
		"!",
		"-",
		"+",
		"-",
		"*",
		"/",
		"%",
		"<<",
		">>",
		"=",
		"+=",
		"-=",
		"*=",
		"/=",
		"%=",
		"<<=",
		">>=",
		"&=",
		"|=",
		"^=",
		"&",
		"|",
		"^",
		"~",
		"++",
		"--",
		"?",
		":",
		"++",
		"--",
		"()",
		"construct",
		"index",
		"empty" };

	return op_names[p_op];
}

const char *ShaderLanguage::token_names[TK_MAX] = {
	"EMPTY",
	"IDENTIFIER",
	"TRUE",
	"FALSE",
	"FLOAT_CONSTANT",
	"INT_CONSTANT",
	"UINT_CONSTANT",
	"STRING_CONSTANT",
	"TYPE_VOID",
	"TYPE_BOOL",
	"TYPE_BVEC2",
	"TYPE_BVEC3",
	"TYPE_BVEC4",
	"TYPE_INT",
	"TYPE_IVEC2",
	"TYPE_IVEC3",
	"TYPE_IVEC4",
	"TYPE_UINT",
	"TYPE_UVEC2",
	"TYPE_UVEC3",
	"TYPE_UVEC4",
	"TYPE_FLOAT",
	"TYPE_VEC2",
	"TYPE_VEC3",
	"TYPE_VEC4",
	"TYPE_MAT2",
	"TYPE_MAT3",
	"TYPE_MAT4",
	"TYPE_SAMPLER2D",
	"TYPE_ISAMPLER2D",
	"TYPE_USAMPLER2D",
	"TYPE_SAMPLER2DARRAY",
	"TYPE_ISAMPLER2DARRAY",
	"TYPE_USAMPLER2DARRAY",
	"TYPE_SAMPLER3D",
	"TYPE_ISAMPLER3D",
	"TYPE_USAMPLER3D",
	"TYPE_SAMPLERCUBE",
	"TYPE_SAMPLERCUBEARRAY",
	"INTERPOLATION_FLAT",
	"INTERPOLATION_SMOOTH",
	"CONST",
	"STRUCT",
	"PRECISION_LOW",
	"PRECISION_MID",
	"PRECISION_HIGH",
	"OP_EQUAL",
	"OP_NOT_EQUAL",
	"OP_LESS",
	"OP_LESS_EQUAL",
	"OP_GREATER",
	"OP_GREATER_EQUAL",
	"OP_AND",
	"OP_OR",
	"OP_NOT",
	"OP_ADD",
	"OP_SUB",
	"OP_MUL",
	"OP_DIV",
	"OP_MOD",
	"OP_SHIFT_LEFT",
	"OP_SHIFT_RIGHT",
	"OP_ASSIGN",
	"OP_ASSIGN_ADD",
	"OP_ASSIGN_SUB",
	"OP_ASSIGN_MUL",
	"OP_ASSIGN_DIV",
	"OP_ASSIGN_MOD",
	"OP_ASSIGN_SHIFT_LEFT",
	"OP_ASSIGN_SHIFT_RIGHT",
	"OP_ASSIGN_BIT_AND",
	"OP_ASSIGN_BIT_OR",
	"OP_ASSIGN_BIT_XOR",
	"OP_BIT_AND",
	"OP_BIT_OR",
	"OP_BIT_XOR",
	"OP_BIT_INVERT",
	"OP_INCREMENT",
	"OP_DECREMENT",
	"CF_IF",
	"CF_ELSE",
	"CF_FOR",
	"CF_WHILE",
	"CF_DO",
	"CF_SWITCH",
	"CF_CASE",
	"CF_DEFAULT",
	"CF_BREAK",
	"CF_CONTINUE",
	"CF_RETURN",
	"CF_DISCARD",
	"BRACKET_OPEN",
	"BRACKET_CLOSE",
	"CURLY_BRACKET_OPEN",
	"CURLY_BRACKET_CLOSE",
	"PARENTHESIS_OPEN",
	"PARENTHESIS_CLOSE",
	"QUESTION",
	"COMMA",
	"COLON",
	"SEMICOLON",
	"PERIOD",
	"UNIFORM",
	"UNIFORM_GROUP",
	"INSTANCE",
	"GLOBAL",
	"VARYING",
	"ARG_IN",
	"ARG_OUT",
	"ARG_INOUT",
	"RENDER_MODE",
	"HINT_DEFAULT_WHITE_TEXTURE",
	"HINT_DEFAULT_BLACK_TEXTURE",
	"HINT_DEFAULT_TRANSPARENT_TEXTURE",
	"HINT_NORMAL_TEXTURE",
	"HINT_ROUGHNESS_NORMAL_TEXTURE",
	"HINT_ROUGHNESS_R",
	"HINT_ROUGHNESS_G",
	"HINT_ROUGHNESS_B",
	"HINT_ROUGHNESS_A",
	"HINT_ROUGHNESS_GRAY",
	"HINT_ANISOTROPY_TEXTURE",
	"HINT_SOURCE_COLOR",
	"HINT_RANGE",
	"HINT_ENUM",
	"HINT_INSTANCE_INDEX",
	"HINT_SCREEN_TEXTURE",
	"HINT_NORMAL_ROUGHNESS_TEXTURE",
	"HINT_DEPTH_TEXTURE",
	"FILTER_NEAREST",
	"FILTER_LINEAR",
	"FILTER_NEAREST_MIPMAP",
	"FILTER_LINEAR_MIPMAP",
	"FILTER_NEAREST_MIPMAP_ANISOTROPIC",
	"FILTER_LINEAR_MIPMAP_ANISOTROPIC",
	"REPEAT_ENABLE",
	"REPEAT_DISABLE",
	"SHADER_TYPE",
	"CURSOR",
	"ERROR",
	"EOF",
};

String ShaderLanguage::get_token_text(Token p_token) {
	String name = token_names[p_token.type];
	if (p_token.is_integer_constant() || p_token.type == TK_FLOAT_CONSTANT) {
		name += "(" + rtos(p_token.constant) + ")";
	} else if (p_token.type == TK_IDENTIFIER) {
		name += "(" + String(p_token.text) + ")";
	} else if (p_token.type == TK_ERROR) {
		name += "(" + String(p_token.text) + ")";
	}

	return name;
}

ShaderLanguage::Token ShaderLanguage::_make_token(TokenType p_type, const StringName &p_text) {
	Token tk;
	tk.type = p_type;
	tk.text = p_text;
	tk.line = tk_line;
	if (tk.type == TK_ERROR) {
		_set_error(p_text);
	}
	return tk;
}

enum ContextFlag : uint32_t {
	CF_UNSPECIFIED = 0U,
	CF_BLOCK = 1U, // "void test() { <x> }"
	CF_FUNC_DECL_PARAM_SPEC = 2U, // "void test(<x> int param) {}"
	CF_FUNC_DECL_PARAM_TYPE = 4U, // "void test(<x> param) {}"
	CF_IF_DECL = 8U, // "if(<x>) {}"
	CF_BOOLEAN = 16U, // "bool t = <x>;"
	CF_GLOBAL_SPACE = 32U, // "struct", "const", "void" etc.
	CF_DATATYPE = 64U, // "<x> value;"
	CF_UNIFORM_TYPE = 128U, // "uniform <x> myUniform;"
	CF_VARYING_TYPE = 256U, // "varying <x> myVarying;"
	CF_PRECISION_MODIFIER = 512U, // "<x> vec4 a = vec4(0.0, 1.0, 2.0, 3.0);"
	CF_INTERPOLATION_QUALIFIER = 1024U, // "varying <x> vec3 myColor;"
	CF_UNIFORM_KEYWORD = 2048U, // "uniform"
	CF_CONST_KEYWORD = 4096U, // "const"
	CF_UNIFORM_QUALIFIER = 8192U, // "<x> uniform float t;"
	CF_SHADER_TYPE = 16384U, // "shader_type"
};

const uint32_t KCF_DATATYPE = CF_BLOCK | CF_GLOBAL_SPACE | CF_DATATYPE | CF_FUNC_DECL_PARAM_TYPE | CF_UNIFORM_TYPE;
const uint32_t KCF_SAMPLER_DATATYPE = CF_FUNC_DECL_PARAM_TYPE | CF_UNIFORM_TYPE;

const ShaderLanguage::KeyWord ShaderLanguage::keyword_list[] = {
	{ TK_TRUE, "true", CF_BLOCK | CF_IF_DECL | CF_BOOLEAN, {}, {} },
	{ TK_FALSE, "false", CF_BLOCK | CF_IF_DECL | CF_BOOLEAN, {}, {} },

	// data types

	{ TK_TYPE_VOID, "void", CF_GLOBAL_SPACE, {}, {} },
	{ TK_TYPE_BOOL, "bool", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_BVEC2, "bvec2", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_BVEC3, "bvec3", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_BVEC4, "bvec4", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_INT, "int", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_IVEC2, "ivec2", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_IVEC3, "ivec3", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_IVEC4, "ivec4", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_UINT, "uint", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_UVEC2, "uvec2", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_UVEC3, "uvec3", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_UVEC4, "uvec4", KCF_DATATYPE, {}, {} },
	{ TK_TYPE_FLOAT, "float", KCF_DATATYPE | CF_VARYING_TYPE, {}, {} },
	{ TK_TYPE_VEC2, "vec2", KCF_DATATYPE | CF_VARYING_TYPE, {}, {} },
	{ TK_TYPE_VEC3, "vec3", KCF_DATATYPE | CF_VARYING_TYPE, {}, {} },
	{ TK_TYPE_VEC4, "vec4", KCF_DATATYPE | CF_VARYING_TYPE, {}, {} },
	{ TK_TYPE_MAT2, "mat2", KCF_DATATYPE | CF_VARYING_TYPE, {}, {} },
	{ TK_TYPE_MAT3, "mat3", KCF_DATATYPE | CF_VARYING_TYPE, {}, {} },
	{ TK_TYPE_MAT4, "mat4", KCF_DATATYPE | CF_VARYING_TYPE, {}, {} },
	{ TK_TYPE_SAMPLER2D, "sampler2D", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_ISAMPLER2D, "isampler2D", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_USAMPLER2D, "usampler2D", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_SAMPLER2DARRAY, "sampler2DArray", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_ISAMPLER2DARRAY, "isampler2DArray", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_USAMPLER2DARRAY, "usampler2DArray", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_SAMPLER3D, "sampler3D", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_ISAMPLER3D, "isampler3D", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_USAMPLER3D, "usampler3D", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_SAMPLERCUBE, "samplerCube", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_SAMPLERCUBEARRAY, "samplerCubeArray", KCF_SAMPLER_DATATYPE, {}, {} },
	{ TK_TYPE_SAMPLEREXT, "samplerExternalOES", KCF_SAMPLER_DATATYPE, {}, {} },

	// interpolation qualifiers

	{ TK_INTERPOLATION_FLAT, "flat", CF_INTERPOLATION_QUALIFIER, {}, {} },
	{ TK_INTERPOLATION_SMOOTH, "smooth", CF_INTERPOLATION_QUALIFIER, {}, {} },

	// precision modifiers

	{ TK_PRECISION_LOW, "lowp", CF_BLOCK | CF_PRECISION_MODIFIER, {}, {} },
	{ TK_PRECISION_MID, "mediump", CF_BLOCK | CF_PRECISION_MODIFIER, {}, {} },
	{ TK_PRECISION_HIGH, "highp", CF_BLOCK | CF_PRECISION_MODIFIER, {}, {} },

	// global space keywords

	{ TK_UNIFORM, "uniform", CF_GLOBAL_SPACE | CF_UNIFORM_KEYWORD, {}, {} },
	{ TK_UNIFORM_GROUP, "group_uniforms", CF_GLOBAL_SPACE, {}, {} },
	{ TK_VARYING, "varying", CF_GLOBAL_SPACE, { "particles", "sky", "fog" }, {} },
	{ TK_CONST, "const", CF_BLOCK | CF_GLOBAL_SPACE | CF_CONST_KEYWORD, {}, {} },
	{ TK_STRUCT, "struct", CF_GLOBAL_SPACE, {}, {} },
	{ TK_SHADER_TYPE, "shader_type", CF_SHADER_TYPE, {}, {} },
	{ TK_RENDER_MODE, "render_mode", CF_GLOBAL_SPACE, {}, {} },

	// uniform qualifiers

	{ TK_INSTANCE, "instance", CF_GLOBAL_SPACE | CF_UNIFORM_QUALIFIER, {}, {} },
	{ TK_GLOBAL, "global", CF_GLOBAL_SPACE | CF_UNIFORM_QUALIFIER, {}, {} },

	// block keywords

	{ TK_CF_IF, "if", CF_BLOCK, {}, {} },
	{ TK_CF_ELSE, "else", CF_BLOCK, {}, {} },
	{ TK_CF_FOR, "for", CF_BLOCK, {}, {} },
	{ TK_CF_WHILE, "while", CF_BLOCK, {}, {} },
	{ TK_CF_DO, "do", CF_BLOCK, {}, {} },
	{ TK_CF_SWITCH, "switch", CF_BLOCK, {}, {} },
	{ TK_CF_CASE, "case", CF_BLOCK, {}, {} },
	{ TK_CF_DEFAULT, "default", CF_BLOCK, {}, {} },
	{ TK_CF_BREAK, "break", CF_BLOCK, {}, {} },
	{ TK_CF_CONTINUE, "continue", CF_BLOCK, {}, {} },
	{ TK_CF_RETURN, "return", CF_BLOCK, {}, {} },
	{ TK_CF_DISCARD, "discard", CF_BLOCK, { "particles", "sky", "fog" }, { "vertex" } },

	// function specifier keywords

	{ TK_ARG_IN, "in", CF_FUNC_DECL_PARAM_SPEC, {}, {} },
	{ TK_ARG_OUT, "out", CF_FUNC_DECL_PARAM_SPEC, {}, {} },
	{ TK_ARG_INOUT, "inout", CF_FUNC_DECL_PARAM_SPEC, {}, {} },

	// hints

	{ TK_HINT_SOURCE_COLOR, "source_color", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_RANGE, "hint_range", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_ENUM, "hint_enum", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_INSTANCE_INDEX, "instance_index", CF_UNSPECIFIED, {}, {} },

	// sampler hints

	{ TK_HINT_NORMAL_TEXTURE, "hint_normal", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_DEFAULT_WHITE_TEXTURE, "hint_default_white", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_DEFAULT_BLACK_TEXTURE, "hint_default_black", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_DEFAULT_TRANSPARENT_TEXTURE, "hint_default_transparent", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_ANISOTROPY_TEXTURE, "hint_anisotropy", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_ROUGHNESS_R, "hint_roughness_r", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_ROUGHNESS_G, "hint_roughness_g", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_ROUGHNESS_B, "hint_roughness_b", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_ROUGHNESS_A, "hint_roughness_a", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_ROUGHNESS_NORMAL_TEXTURE, "hint_roughness_normal", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_ROUGHNESS_GRAY, "hint_roughness_gray", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_SCREEN_TEXTURE, "hint_screen_texture", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_NORMAL_ROUGHNESS_TEXTURE, "hint_normal_roughness_texture", CF_UNSPECIFIED, {}, {} },
	{ TK_HINT_DEPTH_TEXTURE, "hint_depth_texture", CF_UNSPECIFIED, {}, {} },

	{ TK_FILTER_NEAREST, "filter_nearest", CF_UNSPECIFIED, {}, {} },
	{ TK_FILTER_LINEAR, "filter_linear", CF_UNSPECIFIED, {}, {} },
	{ TK_FILTER_NEAREST_MIPMAP, "filter_nearest_mipmap", CF_UNSPECIFIED, {}, {} },
	{ TK_FILTER_LINEAR_MIPMAP, "filter_linear_mipmap", CF_UNSPECIFIED, {}, {} },
	{ TK_FILTER_NEAREST_MIPMAP_ANISOTROPIC, "filter_nearest_mipmap_anisotropic", CF_UNSPECIFIED, {}, {} },
	{ TK_FILTER_LINEAR_MIPMAP_ANISOTROPIC, "filter_linear_mipmap_anisotropic", CF_UNSPECIFIED, {}, {} },
	{ TK_REPEAT_ENABLE, "repeat_enable", CF_UNSPECIFIED, {}, {} },
	{ TK_REPEAT_DISABLE, "repeat_disable", CF_UNSPECIFIED, {}, {} },

	{ TK_ERROR, nullptr, CF_UNSPECIFIED, {}, {} }
};

ShaderLanguage::Token ShaderLanguage::_get_token() {
#define GETCHAR(m_idx) (((char_idx + m_idx) < code.length()) ? code[char_idx + m_idx] : char32_t(0))

	while (true) {
		char_idx++;
		switch (GETCHAR(-1)) {
			case 0:
				return _make_token(TK_EOF);
			case 0xFFFF:
				return _make_token(TK_CURSOR); //for completion
			case '\t':
			case '\r':
			case ' ':
				continue;
			case '\n':
				tk_line++;
				continue;
			case '/': {
				switch (GETCHAR(0)) {
					case '*': { // block comment

						char_idx++;
						while (true) {
							if (GETCHAR(0) == 0) {
								return _make_token(TK_EOF);
							}
							if (GETCHAR(0) == '*' && GETCHAR(1) == '/') {
								char_idx += 2;
								break;
							} else if (GETCHAR(0) == '\n') {
								tk_line++;
							}

							char_idx++;
						}

					} break;
					case '/': { // line comment skip

						while (true) {
							if (GETCHAR(0) == '\n') {
								tk_line++;
								char_idx++;
								break;
							}
							if (GETCHAR(0) == 0) {
								return _make_token(TK_EOF);
							}
							char_idx++;
						}

					} break;
					case '=': { // diveq

						char_idx++;
						return _make_token(TK_OP_ASSIGN_DIV);

					} break;
					default:
						return _make_token(TK_OP_DIV);
				}

				continue; //a comment, continue to next token
			} break;
			case '=': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_EQUAL);
				}

				return _make_token(TK_OP_ASSIGN);

			} break;
			case '<': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_LESS_EQUAL);
				} else if (GETCHAR(0) == '<') {
					char_idx++;
					if (GETCHAR(0) == '=') {
						char_idx++;
						return _make_token(TK_OP_ASSIGN_SHIFT_LEFT);
					}

					return _make_token(TK_OP_SHIFT_LEFT);
				}

				return _make_token(TK_OP_LESS);

			} break;
			case '>': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_GREATER_EQUAL);
				} else if (GETCHAR(0) == '>') {
					char_idx++;
					if (GETCHAR(0) == '=') {
						char_idx++;
						return _make_token(TK_OP_ASSIGN_SHIFT_RIGHT);
					}

					return _make_token(TK_OP_SHIFT_RIGHT);
				}

				return _make_token(TK_OP_GREATER);

			} break;
			case '!': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_NOT_EQUAL);
				}

				return _make_token(TK_OP_NOT);

			} break;
			case '"': {
				String _content = "";
				bool _previous_backslash = false;

				while (true) {
					bool _ended = false;
					char32_t c = GETCHAR(0);
					if (c == 0) {
						return _make_token(TK_ERROR, "EOF reached before string termination.");
					}
					switch (c) {
						case '"': {
							if (_previous_backslash) {
								_content += '"';
								_previous_backslash = false;
							} else {
								_ended = true;
							}
							break;
						}
						case '\\': {
							if (_previous_backslash) {
								_content += '\\';
							}
							_previous_backslash = !_previous_backslash;
							break;
						}
						case '\n': {
							return _make_token(TK_ERROR, "Unexpected end of string.");
						}
						default: {
							if (!_previous_backslash) {
								_content += c;
							} else {
								return _make_token(TK_ERROR, "Only \\\" and \\\\ escape characters supported.");
							}
							break;
						}
					}

					char_idx++;
					if (_ended) {
						break;
					}
				}

				return _make_token(TK_STRING_CONSTANT, _content);
			} break;
			//case '\'' //string - no strings in shader
			case '{':
				return _make_token(TK_CURLY_BRACKET_OPEN);
			case '}':
				return _make_token(TK_CURLY_BRACKET_CLOSE);
			case '[':
				return _make_token(TK_BRACKET_OPEN);
			case ']':
				return _make_token(TK_BRACKET_CLOSE);
			case '(':
				return _make_token(TK_PARENTHESIS_OPEN);
			case ')':
				return _make_token(TK_PARENTHESIS_CLOSE);
			case ',':
				return _make_token(TK_COMMA);
			case ';':
				return _make_token(TK_SEMICOLON);
			case '?':
				return _make_token(TK_QUESTION);
			case ':':
				return _make_token(TK_COLON);
			case '^':
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_ASSIGN_BIT_XOR);
				}
				return _make_token(TK_OP_BIT_XOR);
			case '~':
				return _make_token(TK_OP_BIT_INVERT);
			case '&': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_ASSIGN_BIT_AND);
				} else if (GETCHAR(0) == '&') {
					char_idx++;
					return _make_token(TK_OP_AND);
				}
				return _make_token(TK_OP_BIT_AND);
			} break;
			case '|': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_ASSIGN_BIT_OR);
				} else if (GETCHAR(0) == '|') {
					char_idx++;
					return _make_token(TK_OP_OR);
				}
				return _make_token(TK_OP_BIT_OR);

			} break;
			case '*': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_ASSIGN_MUL);
				}
				return _make_token(TK_OP_MUL);
			} break;
			case '+': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_ASSIGN_ADD);
				} else if (GETCHAR(0) == '+') {
					char_idx++;
					return _make_token(TK_OP_INCREMENT);
				}

				return _make_token(TK_OP_ADD);
			} break;
			case '-': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_ASSIGN_SUB);
				} else if (GETCHAR(0) == '-') {
					char_idx++;
					return _make_token(TK_OP_DECREMENT);
				}

				return _make_token(TK_OP_SUB);
			} break;
			case '%': {
				if (GETCHAR(0) == '=') {
					char_idx++;
					return _make_token(TK_OP_ASSIGN_MOD);
				}

				return _make_token(TK_OP_MOD);
			} break;
			case '@': {
				if (GETCHAR(0) == '@' && GETCHAR(1) == '>') {
					char_idx += 2;

					LocalVector<char32_t> incp;
					while (GETCHAR(0) != '\n') {
						incp.push_back(GETCHAR(0));
						char_idx++;
					}
					incp.push_back(0); // Zero end it.
					String include_path(incp.ptr());
					include_positions.write[include_positions.size() - 1].line = tk_line;

					String marker = ">>" + include_path;
					if (!include_markers_handled.has(marker)) {
						include_markers_handled.insert(marker);

						FilePosition fp;
						fp.file = include_path;
						fp.line = 0;
						tk_line = 0;
						include_positions.push_back(fp);
					}

				} else if (GETCHAR(0) == '@' && GETCHAR(1) == '<') {
					char_idx += 2;

					LocalVector<char32_t> incp;
					while (GETCHAR(0) != '\n') {
						incp.push_back(GETCHAR(0));
						char_idx++;
					}
					incp.push_back(0); // Zero end it.
					String include_path(incp.ptr());

					String marker = "<<" + include_path;
					if (!include_markers_handled.has(marker)) {
						include_markers_handled.insert(marker);

						if (include_positions.size() == 1) {
							return _make_token(TK_ERROR, "Invalid include exit hint @@< without matching enter hint.");
						}
						include_positions.resize(include_positions.size() - 1); // Pop back.
					}

					tk_line = include_positions[include_positions.size() - 1].line - 1; // Restore line.

				} else {
					return _make_token(TK_ERROR, "Invalid include enter/exit hint token (@@> and @@<)");
				}
			} break;
			default: {
				char_idx--; //go back one, since we have no idea what this is

				if (is_digit(GETCHAR(0)) || (GETCHAR(0) == '.' && is_digit(GETCHAR(1)))) {
					// parse number
					bool hexa_found = false;
					bool period_found = false;
					bool exponent_found = false;
					bool float_suffix_found = false;
					bool uint_suffix_found = false;
					bool end_suffix_found = false;

					enum {
						CASE_ALL,
						CASE_HEXA_PERIOD,
						CASE_EXPONENT,
						CASE_SIGN_AFTER_EXPONENT,
						CASE_NONE,
						CASE_MAX,
					} lut_case = CASE_ALL;

					static bool suffix_lut[CASE_MAX][127];

					if (!is_const_suffix_lut_initialized) {
						is_const_suffix_lut_initialized = true;

						for (int i = 0; i < 127; i++) {
							char t = char(i);

							suffix_lut[CASE_ALL][i] = t == '.' || t == 'x' || t == 'e' || t == 'f' || t == 'u' || t == '-' || t == '+';
							suffix_lut[CASE_HEXA_PERIOD][i] = t == 'e' || t == 'f' || t == 'u';
							suffix_lut[CASE_EXPONENT][i] = t == 'f' || t == '-' || t == '+';
							suffix_lut[CASE_SIGN_AFTER_EXPONENT][i] = t == 'f';
							suffix_lut[CASE_NONE][i] = false;
						}
					}

					String str;
					int i = 0;
					bool digit_after_exp = false;

					while (true) {
						const char32_t symbol = String::char_lowercase(GETCHAR(i));
						bool error = false;

						if (is_digit(symbol)) {
							if (exponent_found) {
								digit_after_exp = true;
							}
							if (end_suffix_found) {
								error = true;
							}
						} else {
							if (symbol < 0x7F && suffix_lut[lut_case][symbol]) {
								if (symbol == 'x') {
									hexa_found = true;
									lut_case = CASE_HEXA_PERIOD;
								} else if (symbol == '.') {
									period_found = true;
									lut_case = CASE_HEXA_PERIOD;
								} else if (symbol == 'e' && !hexa_found) {
									exponent_found = true;
									lut_case = CASE_EXPONENT;
								} else if (symbol == 'f' && !hexa_found) {
									if (!period_found && !exponent_found) {
										error = true;
									}
									float_suffix_found = true;
									end_suffix_found = true;
									lut_case = CASE_NONE;
								} else if (symbol == 'u') {
									uint_suffix_found = true;
									end_suffix_found = true;
									lut_case = CASE_NONE;
								} else if (symbol == '-' || symbol == '+') {
									if (exponent_found) {
										lut_case = CASE_SIGN_AFTER_EXPONENT;
									} else {
										break;
									}
								}
							} else if (!hexa_found || !is_hex_digit(symbol)) {
								if (is_ascii_identifier_char(symbol)) {
									error = true;
								} else {
									break;
								}
							}
						}

						if (error) {
							if (hexa_found) {
								return _make_token(TK_ERROR, "Invalid (hexadecimal) numeric constant");
							}
							if (period_found || exponent_found || float_suffix_found) {
								return _make_token(TK_ERROR, "Invalid (float) numeric constant");
							}
							if (uint_suffix_found) {
								return _make_token(TK_ERROR, "Invalid (unsigned integer) numeric constant");
							}
							return _make_token(TK_ERROR, "Invalid (integer) numeric constant");
						}
						str += symbol;
						i++;
					}

					char32_t last_char = str[str.length() - 1];

					if (hexa_found) { // Integer (hex).
						if (uint_suffix_found) {
							// Strip the suffix.
							str = str.left(str.length() - 1);

							// Compensate reading cursor position.
							char_idx += 1;
						}
						if (str.size() > 11 || !str.is_valid_hex_number(true)) { // > 0xFFFFFFFF
							return _make_token(TK_ERROR, "Invalid (hexadecimal) numeric constant");
						}
					} else if (period_found || exponent_found || float_suffix_found) { // Float
						if (exponent_found && (!digit_after_exp || (!is_digit(last_char) && last_char != 'f'))) { // Checks for eg: "2E", "2E-", "2E+" and 0ef, 0e+f, 0.0ef, 0.0e-f (exponent without digit after it).
							return _make_token(TK_ERROR, "Invalid (float) numeric constant");
						}
						if (period_found) {
							if (float_suffix_found) {
								//checks for eg "1.f" or "1.99f" notations
								if (last_char != 'f') {
									return _make_token(TK_ERROR, "Invalid (float) numeric constant");
								}
							} else {
								//checks for eg. "1." or "1.99" notations
								if (last_char != '.' && !is_digit(last_char)) {
									return _make_token(TK_ERROR, "Invalid (float) numeric constant");
								}
							}
						} else if (float_suffix_found) {
							// if no period found the float suffix must be the last character, like in "2f" for "2.0"
							if (last_char != 'f') {
								return _make_token(TK_ERROR, "Invalid (float) numeric constant");
							}
						}

						if (float_suffix_found) {
							// Strip the suffix.
							str = str.left(str.length() - 1);
							// Compensate reading cursor position.
							char_idx += 1;
						}

						if (!str.is_valid_float()) {
							return _make_token(TK_ERROR, "Invalid (float) numeric constant");
						}
					} else { // Integer
						if (uint_suffix_found) {
							// Strip the suffix.
							str = str.left(str.length() - 1);
							// Compensate reading cursor position.
							char_idx += 1;
						}
						if (!str.is_valid_int()) {
							if (uint_suffix_found) {
								return _make_token(TK_ERROR, "Invalid (unsigned integer) numeric constant");
							} else {
								return _make_token(TK_ERROR, "Invalid (integer) numeric constant");
							}
						}
					}

					char_idx += str.length();
					Token tk;
					if (period_found || exponent_found || float_suffix_found) {
						tk.type = TK_FLOAT_CONSTANT;
					} else if (uint_suffix_found) {
						tk.type = TK_UINT_CONSTANT;
					} else {
						tk.type = TK_INT_CONSTANT;
					}

					if (hexa_found) {
						tk.constant = (double)str.hex_to_int();
					} else {
						tk.constant = str.to_float();
					}
					tk.line = tk_line;

					return tk;
				}

				if (GETCHAR(0) == '.') {
					//parse period
					char_idx++;
					return _make_token(TK_PERIOD);
				}

				if (is_ascii_identifier_char(GETCHAR(0))) {
					// parse identifier
					String str;

					while (is_ascii_identifier_char(GETCHAR(0))) {
						str += char32_t(GETCHAR(0));
						char_idx++;
					}

					//see if keyword
					//should be converted to a static map
					int idx = 0;

					while (keyword_list[idx].text) {
						if (str == keyword_list[idx].text) {
							return _make_token(keyword_list[idx].token);
						}
						idx++;
					}

					str = str.replace("dus_", "_");

					return _make_token(TK_IDENTIFIER, str);
				}

				if (GETCHAR(0) > 32) {
					return _make_token(TK_ERROR, "Tokenizer: Unknown character #" + itos(GETCHAR(0)) + ": '" + String::chr(GETCHAR(0)) + "'");
				} else {
					return _make_token(TK_ERROR, "Tokenizer: Unknown character #" + itos(GETCHAR(0)));
				}

			} break;
		}
	}
	ERR_PRINT("BUG");
	return Token();

#undef GETCHAR
}

bool ShaderLanguage::_lookup_next(Token &r_tk) {
	TkPos pre_pos = _get_tkpos();
	int line = pre_pos.tk_line;
	_get_token();
	Token tk = _get_token();
	_set_tkpos(pre_pos);
	if (tk.line == line) {
		r_tk = tk;
		return true;
	}
	return false;
}

ShaderLanguage::Token ShaderLanguage::_peek() {
	TkPos pre_pos = _get_tkpos();
	Token tk = _get_token();
	_set_tkpos(pre_pos);
	return tk;
}

String ShaderLanguage::token_debug(const String &p_code) {
	clear();

	code = p_code;

	String output;

	Token tk = _get_token();
	while (tk.type != TK_EOF && tk.type != TK_ERROR) {
		output += itos(tk_line) + ": " + get_token_text(tk) + "\n";
		tk = _get_token();
	}

	return output;
}

bool ShaderLanguage::is_token_variable_datatype(TokenType p_type) {
	return (
			p_type == TK_TYPE_VOID ||
			p_type == TK_TYPE_BOOL ||
			p_type == TK_TYPE_BVEC2 ||
			p_type == TK_TYPE_BVEC3 ||
			p_type == TK_TYPE_BVEC4 ||
			p_type == TK_TYPE_INT ||
			p_type == TK_TYPE_IVEC2 ||
			p_type == TK_TYPE_IVEC3 ||
			p_type == TK_TYPE_IVEC4 ||
			p_type == TK_TYPE_UINT ||
			p_type == TK_TYPE_UVEC2 ||
			p_type == TK_TYPE_UVEC3 ||
			p_type == TK_TYPE_UVEC4 ||
			p_type == TK_TYPE_FLOAT ||
			p_type == TK_TYPE_VEC2 ||
			p_type == TK_TYPE_VEC3 ||
			p_type == TK_TYPE_VEC4 ||
			p_type == TK_TYPE_MAT2 ||
			p_type == TK_TYPE_MAT3 ||
			p_type == TK_TYPE_MAT4);
}

bool ShaderLanguage::is_token_datatype(TokenType p_type) {
	return (
			p_type == TK_TYPE_VOID ||
			p_type == TK_TYPE_BOOL ||
			p_type == TK_TYPE_BVEC2 ||
			p_type == TK_TYPE_BVEC3 ||
			p_type == TK_TYPE_BVEC4 ||
			p_type == TK_TYPE_INT ||
			p_type == TK_TYPE_IVEC2 ||
			p_type == TK_TYPE_IVEC3 ||
			p_type == TK_TYPE_IVEC4 ||
			p_type == TK_TYPE_UINT ||
			p_type == TK_TYPE_UVEC2 ||
			p_type == TK_TYPE_UVEC3 ||
			p_type == TK_TYPE_UVEC4 ||
			p_type == TK_TYPE_FLOAT ||
			p_type == TK_TYPE_VEC2 ||
			p_type == TK_TYPE_VEC3 ||
			p_type == TK_TYPE_VEC4 ||
			p_type == TK_TYPE_MAT2 ||
			p_type == TK_TYPE_MAT3 ||
			p_type == TK_TYPE_MAT4 ||
			p_type == TK_TYPE_SAMPLER2D ||
			p_type == TK_TYPE_ISAMPLER2D ||
			p_type == TK_TYPE_USAMPLER2D ||
			p_type == TK_TYPE_SAMPLER2DARRAY ||
			p_type == TK_TYPE_ISAMPLER2DARRAY ||
			p_type == TK_TYPE_USAMPLER2DARRAY ||
			p_type == TK_TYPE_SAMPLER3D ||
			p_type == TK_TYPE_ISAMPLER3D ||
			p_type == TK_TYPE_USAMPLER3D ||
			p_type == TK_TYPE_SAMPLERCUBE ||
			p_type == TK_TYPE_SAMPLERCUBEARRAY ||
			p_type == TK_TYPE_SAMPLEREXT);
}

ShaderLanguage::DataType ShaderLanguage::get_token_datatype(TokenType p_type) {
	return DataType(p_type - TK_TYPE_VOID);
}

bool ShaderLanguage::is_token_interpolation(TokenType p_type) {
	return (
			p_type == TK_INTERPOLATION_FLAT ||
			p_type == TK_INTERPOLATION_SMOOTH);
}

ShaderLanguage::DataInterpolation ShaderLanguage::get_token_interpolation(TokenType p_type) {
	if (p_type == TK_INTERPOLATION_FLAT) {
		return INTERPOLATION_FLAT;
	} else {
		return INTERPOLATION_SMOOTH;
	}
}

bool ShaderLanguage::is_token_precision(TokenType p_type) {
	return (
			p_type == TK_PRECISION_LOW ||
			p_type == TK_PRECISION_MID ||
			p_type == TK_PRECISION_HIGH);
}

bool ShaderLanguage::is_token_arg_qual(TokenType p_type) {
	return (
			p_type == TK_ARG_IN ||
			p_type == TK_ARG_OUT ||
			p_type == TK_ARG_INOUT);
}

ShaderLanguage::DataPrecision ShaderLanguage::get_token_precision(TokenType p_type) {
	if (p_type == TK_PRECISION_LOW) {
		return PRECISION_LOWP;
	} else if (p_type == TK_PRECISION_HIGH) {
		return PRECISION_HIGHP;
	} else {
		return PRECISION_MEDIUMP;
	}
}

String ShaderLanguage::get_precision_name(DataPrecision p_type) {
	switch (p_type) {
		case PRECISION_LOWP:
			return "lowp";
		case PRECISION_MEDIUMP:
			return "mediump";
		case PRECISION_HIGHP:
			return "highp";
		default:
			break;
	}
	return "";
}

String ShaderLanguage::get_interpolation_name(DataInterpolation p_interpolation) {
	switch (p_interpolation) {
		case INTERPOLATION_FLAT:
			return "flat";
		case INTERPOLATION_SMOOTH:
			return "smooth";
		default:
			break;
	}
	return "";
}

String ShaderLanguage::get_datatype_name(DataType p_type) {
	switch (p_type) {
		case TYPE_VOID:
			return "void";
		case TYPE_BOOL:
			return "bool";
		case TYPE_BVEC2:
			return "bvec2";
		case TYPE_BVEC3:
			return "bvec3";
		case TYPE_BVEC4:
			return "bvec4";
		case TYPE_INT:
			return "int";
		case TYPE_IVEC2:
			return "ivec2";
		case TYPE_IVEC3:
			return "ivec3";
		case TYPE_IVEC4:
			return "ivec4";
		case TYPE_UINT:
			return "uint";
		case TYPE_UVEC2:
			return "uvec2";
		case TYPE_UVEC3:
			return "uvec3";
		case TYPE_UVEC4:
			return "uvec4";
		case TYPE_FLOAT:
			return "float";
		case TYPE_VEC2:
			return "vec2";
		case TYPE_VEC3:
			return "vec3";
		case TYPE_VEC4:
			return "vec4";
		case TYPE_MAT2:
			return "mat2";
		case TYPE_MAT3:
			return "mat3";
		case TYPE_MAT4:
			return "mat4";
		case TYPE_SAMPLER2D:
			return "sampler2D";
		case TYPE_ISAMPLER2D:
			return "isampler2D";
		case TYPE_USAMPLER2D:
			return "usampler2D";
		case TYPE_SAMPLER2DARRAY:
			return "sampler2DArray";
		case TYPE_ISAMPLER2DARRAY:
			return "isampler2DArray";
		case TYPE_USAMPLER2DARRAY:
			return "usampler2DArray";
		case TYPE_SAMPLER3D:
			return "sampler3D";
		case TYPE_ISAMPLER3D:
			return "isampler3D";
		case TYPE_USAMPLER3D:
			return "usampler3D";
		case TYPE_SAMPLERCUBE:
			return "samplerCube";
		case TYPE_SAMPLERCUBEARRAY:
			return "samplerCubeArray";
		case TYPE_SAMPLEREXT:
			return "samplerExternalOES";
		case TYPE_STRUCT:
			return "struct";
		case TYPE_MAX:
			return "invalid";
	}

	return "";
}

String ShaderLanguage::get_uniform_hint_name(ShaderNode::Uniform::Hint p_hint) {
	String result;
	switch (p_hint) {
		case ShaderNode::Uniform::HINT_RANGE: {
			result = "hint_range";
		} break;
		case ShaderNode::Uniform::HINT_ENUM: {
			result = "hint_enum";
		} break;
		case ShaderNode::Uniform::HINT_SOURCE_COLOR: {
			result = "source_color";
		} break;
		case ShaderNode::Uniform::HINT_NORMAL: {
			result = "hint_normal";
		} break;
		case ShaderNode::Uniform::HINT_ROUGHNESS_NORMAL: {
			result = "hint_roughness_normal";
		} break;
		case ShaderNode::Uniform::HINT_ROUGHNESS_R: {
			result = "hint_roughness_r";
		} break;
		case ShaderNode::Uniform::HINT_ROUGHNESS_G: {
			result = "hint_roughness_g";
		} break;
		case ShaderNode::Uniform::HINT_ROUGHNESS_B: {
			result = "hint_roughness_b";
		} break;
		case ShaderNode::Uniform::HINT_ROUGHNESS_A: {
			result = "hint_roughness_a";
		} break;
		case ShaderNode::Uniform::HINT_ROUGHNESS_GRAY: {
			result = "hint_roughness_gray";
		} break;
		case ShaderNode::Uniform::HINT_DEFAULT_BLACK: {
			result = "hint_default_black";
		} break;
		case ShaderNode::Uniform::HINT_DEFAULT_WHITE: {
			result = "hint_default_white";
		} break;
		case ShaderNode::Uniform::HINT_DEFAULT_TRANSPARENT: {
			result = "hint_default_transparent";
		} break;
		case ShaderNode::Uniform::HINT_ANISOTROPY: {
			result = "hint_anisotropy";
		} break;
		case ShaderNode::Uniform::HINT_SCREEN_TEXTURE: {
			result = "hint_screen_texture";
		} break;
		case ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE: {
			result = "hint_normal_roughness_texture";
		} break;
		case ShaderNode::Uniform::HINT_DEPTH_TEXTURE: {
			result = "hint_depth_texture";
		} break;
		default:
			break;
	}
	return result;
}

String ShaderLanguage::get_texture_filter_name(TextureFilter p_filter) {
	String result;
	switch (p_filter) {
		case FILTER_NEAREST: {
			result = "filter_nearest";
		} break;
		case FILTER_LINEAR: {
			result = "filter_linear";
		} break;
		case FILTER_NEAREST_MIPMAP: {
			result = "filter_nearest_mipmap";
		} break;
		case FILTER_LINEAR_MIPMAP: {
			result = "filter_linear_mipmap";
		} break;
		case FILTER_NEAREST_MIPMAP_ANISOTROPIC: {
			result = "filter_nearest_mipmap_anisotropic";
		} break;
		case FILTER_LINEAR_MIPMAP_ANISOTROPIC: {
			result = "filter_linear_mipmap_anisotropic";
		} break;
		default: {
		} break;
	}
	return result;
}

String ShaderLanguage::get_texture_repeat_name(TextureRepeat p_repeat) {
	String result;
	switch (p_repeat) {
		case REPEAT_DISABLE: {
			result = "repeat_disable";
		} break;
		case REPEAT_ENABLE: {
			result = "repeat_enable";
		} break;
		default: {
		} break;
	}
	return result;
}

bool ShaderLanguage::is_token_nonvoid_datatype(TokenType p_type) {
	return is_token_datatype(p_type) && p_type != TK_TYPE_VOID;
}

void ShaderLanguage::clear() {
	current_function = StringName();
	last_name = StringName();
	last_type = IDENTIFIER_MAX;
	current_uniform_group_name = "";
	current_uniform_subgroup_name = "";
	current_uniform_hint = ShaderNode::Uniform::HINT_NONE;
	current_uniform_filter = FILTER_DEFAULT;
	current_uniform_repeat = REPEAT_DEFAULT;
	current_uniform_instance_index_defined = false;

	completion_type = COMPLETION_NONE;
	completion_block = nullptr;
	completion_function = StringName();
	completion_class = TAG_GLOBAL;
	completion_struct = StringName();
	completion_base = TYPE_VOID;
	completion_base_array = false;

	include_positions.clear();
	include_positions.push_back(FilePosition());

	include_markers_handled.clear();
	calls_info.clear();
	function_overload_count.clear();

#ifdef DEBUG_ENABLED
	keyword_completion_context = CF_UNSPECIFIED;
	used_constants.clear();
	used_varyings.clear();
	used_uniforms.clear();
	used_functions.clear();
	used_structs.clear();
	used_local_vars.clear();
	warnings.clear();
#endif // DEBUG_ENABLED

	error_line = 0;
	tk_line = 1;
	char_idx = 0;
	error_set = false;
	error_str = "";
	is_const_decl = false;
	while (nodes) {
		Node *n = nodes;
		nodes = nodes->next;
		memdelete(n);
	}
}

#ifdef DEBUG_ENABLED
void ShaderLanguage::_parse_used_identifier(const StringName &p_identifier, IdentifierType p_type, const StringName &p_function) {
	switch (p_type) {
		case IdentifierType::IDENTIFIER_CONSTANT:
			if (HAS_WARNING(ShaderWarning::UNUSED_CONSTANT_FLAG) && used_constants.has(p_identifier)) {
				used_constants[p_identifier].used = true;
			}
			break;
		case IdentifierType::IDENTIFIER_VARYING:
			if (HAS_WARNING(ShaderWarning::UNUSED_VARYING_FLAG) && used_varyings.has(p_identifier)) {
				if (shader->varyings[p_identifier].stage != ShaderNode::Varying::STAGE_VERTEX && shader->varyings[p_identifier].stage != ShaderNode::Varying::STAGE_FRAGMENT) {
					used_varyings[p_identifier].used = true;
				}
			}
			break;
		case IdentifierType::IDENTIFIER_UNIFORM:
			if (HAS_WARNING(ShaderWarning::UNUSED_UNIFORM_FLAG) && used_uniforms.has(p_identifier)) {
				used_uniforms[p_identifier].used = true;
			}
			break;
		case IdentifierType::IDENTIFIER_FUNCTION:
			if (HAS_WARNING(ShaderWarning::UNUSED_FUNCTION_FLAG) && used_functions.has(p_identifier)) {
				used_functions[p_identifier].used = true;
			}
			break;
		case IdentifierType::IDENTIFIER_LOCAL_VAR:
			if (HAS_WARNING(ShaderWarning::UNUSED_LOCAL_VARIABLE_FLAG) && used_local_vars.has(p_function) && used_local_vars[p_function].has(p_identifier)) {
				used_local_vars[p_function][p_identifier].used = true;
			}
			break;
		default:
			break;
	}
}
#endif // DEBUG_ENABLED

bool ShaderLanguage::_find_identifier(const BlockNode *p_block, bool p_allow_reassign, const FunctionInfo &p_function_info, const StringName &p_identifier, DataType *r_data_type, IdentifierType *r_type, bool *r_is_const, int *r_array_size, StringName *r_struct_name, Vector<Scalar> *r_constant_values) {
	if (is_shader_inc) {
		for (int i = 0; i < RenderingServer::SHADER_MAX; i++) {
			for (const KeyValue<StringName, FunctionInfo> &E : ShaderTypes::get_singleton()->get_functions(RenderingServer::ShaderMode(i))) {
				if ((current_function == E.key || E.key == "global" || E.key == "constants") && E.value.built_ins.has(p_identifier)) {
					if (r_data_type) {
						*r_data_type = E.value.built_ins[p_identifier].type;
					}
					if (r_is_const) {
						*r_is_const = E.value.built_ins[p_identifier].constant;
					}
					if (r_type) {
						*r_type = IDENTIFIER_BUILTIN_VAR;
					}
					return true;
				}
			}
		}
	} else {
		if (p_function_info.built_ins.has(p_identifier)) {
			if (r_data_type) {
				*r_data_type = p_function_info.built_ins[p_identifier].type;
			}
			if (r_is_const) {
				*r_is_const = p_function_info.built_ins[p_identifier].constant;
			}
			if (r_type) {
				*r_type = IDENTIFIER_BUILTIN_VAR;
			}
			return true;
		}
	}

	if (p_function_info.stage_functions.has(p_identifier)) {
		if (r_data_type) {
			*r_data_type = p_function_info.stage_functions[p_identifier].return_type;
		}
		if (r_is_const) {
			*r_is_const = true;
		}
		if (r_type) {
			*r_type = IDENTIFIER_FUNCTION;
		}
		return true;
	}

	FunctionNode *function = nullptr;

	while (p_block) {
		if (p_block->variables.has(p_identifier)) {
			if (r_data_type) {
				*r_data_type = p_block->variables[p_identifier].type;
			}
			if (r_is_const) {
				*r_is_const = p_block->variables[p_identifier].is_const;
			}
			if (r_array_size) {
				*r_array_size = p_block->variables[p_identifier].array_size;
			}
			if (r_struct_name) {
				*r_struct_name = p_block->variables[p_identifier].struct_name;
			}
			if (r_constant_values && !p_block->variables[p_identifier].values.is_empty()) {
				*r_constant_values = p_block->variables[p_identifier].values;
			}
			if (r_type) {
				*r_type = IDENTIFIER_LOCAL_VAR;
			}
			return true;
		}

		if (p_block->parent_function) {
			function = p_block->parent_function;
			break;
		} else {
			if (p_allow_reassign) {
				break;
			}
			ERR_FAIL_NULL_V(p_block->parent_block, false);
			p_block = p_block->parent_block;
		}
	}

	if (function) {
		for (int i = 0; i < function->arguments.size(); i++) {
			if (function->arguments[i].name == p_identifier) {
				if (r_data_type) {
					*r_data_type = function->arguments[i].type;
				}
				if (r_struct_name) {
					*r_struct_name = function->arguments[i].struct_name;
				}
				if (r_array_size) {
					*r_array_size = function->arguments[i].array_size;
				}
				if (r_is_const) {
					*r_is_const = function->arguments[i].is_const;
				}
				if (r_type) {
					*r_type = IDENTIFIER_FUNCTION_ARGUMENT;
				}
				return true;
			}
		}
	}

	if (shader->varyings.has(p_identifier)) {
		if (r_data_type) {
			*r_data_type = shader->varyings[p_identifier].type;
		}
		if (r_array_size) {
			*r_array_size = shader->varyings[p_identifier].array_size;
		}
		if (r_type) {
			*r_type = IDENTIFIER_VARYING;
		}
		return true;
	}

	if (shader->uniforms.has(p_identifier)) {
		if (r_data_type) {
			*r_data_type = shader->uniforms[p_identifier].type;
		}
		if (r_array_size) {
			*r_array_size = shader->uniforms[p_identifier].array_size;
		}
		if (r_type) {
			*r_type = IDENTIFIER_UNIFORM;
		}
		return true;
	}

	if (shader->constants.has(p_identifier)) {
		if (r_is_const) {
			*r_is_const = true;
		}
		if (r_data_type) {
			*r_data_type = shader->constants[p_identifier].type;
		}
		if (r_array_size) {
			*r_array_size = shader->constants[p_identifier].array_size;
		}
		if (r_struct_name) {
			*r_struct_name = shader->constants[p_identifier].struct_name;
		}
		if (r_constant_values) {
			if (shader->constants[p_identifier].initializer && !shader->constants[p_identifier].initializer->get_values().is_empty()) {
				*r_constant_values = shader->constants[p_identifier].initializer->get_values();
			}
		}
		if (r_type) {
			*r_type = IDENTIFIER_CONSTANT;
		}
		return true;
	}

	for (int i = 0; i < shader->vfunctions.size(); i++) {
		if (!shader->vfunctions[i].callable) {
			continue;
		}

		if (shader->vfunctions[i].name == p_identifier) {
			if (r_data_type) {
				*r_data_type = shader->vfunctions[i].function->return_type;
			}
			if (r_array_size) {
				*r_array_size = shader->vfunctions[i].function->return_array_size;
			}
			if (r_type) {
				*r_type = IDENTIFIER_FUNCTION;
			}
			return true;
		}
	}

	return false;
}

bool ShaderLanguage::_validate_operator(const BlockNode *p_block, OperatorNode *p_op, DataType *r_ret_type, int *r_ret_size) {
	bool valid = false;
	DataType ret_type = TYPE_VOID;
	int ret_size = 0;

	switch (p_op->op) {
		case OP_EQUAL:
		case OP_NOT_EQUAL: {
			if ((!p_op->arguments[0]->is_indexed() && p_op->arguments[0]->get_array_size() > 0) || (!p_op->arguments[1]->is_indexed() && p_op->arguments[1]->get_array_size() > 0)) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();
			valid = na == nb;
			ret_type = TYPE_BOOL;
		} break;
		case OP_LESS:
		case OP_LESS_EQUAL:
		case OP_GREATER:
		case OP_GREATER_EQUAL: {
			if ((!p_op->arguments[0]->is_indexed() && p_op->arguments[0]->get_array_size() > 0) || (!p_op->arguments[1]->is_indexed() && p_op->arguments[1]->get_array_size() > 0)) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();

			valid = na == nb && (na == TYPE_UINT || na == TYPE_INT || na == TYPE_FLOAT);
			ret_type = TYPE_BOOL;

		} break;
		case OP_AND:
		case OP_OR: {
			if ((!p_op->arguments[0]->is_indexed() && p_op->arguments[0]->get_array_size() > 0) || (!p_op->arguments[1]->is_indexed() && p_op->arguments[1]->get_array_size() > 0)) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();

			valid = na == nb && na == TYPE_BOOL;
			ret_type = TYPE_BOOL;

		} break;
		case OP_NOT: {
			if (!p_op->arguments[0]->is_indexed() && p_op->arguments[0]->get_array_size() > 0) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			valid = na == TYPE_BOOL;
			ret_type = TYPE_BOOL;

		} break;
		case OP_INCREMENT:
		case OP_DECREMENT:
		case OP_POST_INCREMENT:
		case OP_POST_DECREMENT:
		case OP_NEGATE: {
			if (!p_op->arguments[0]->is_indexed() && p_op->arguments[0]->get_array_size() > 0) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			valid = na > TYPE_BVEC4 && na < TYPE_MAT2;
			ret_type = na;
		} break;
		case OP_ADD:
		case OP_SUB:
		case OP_MUL:
		case OP_DIV: {
			if ((!p_op->arguments[0]->is_indexed() && p_op->arguments[0]->get_array_size() > 0) || (!p_op->arguments[1]->is_indexed() && p_op->arguments[1]->get_array_size() > 0)) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();

			if (na > nb) {
				//make things easier;
				SWAP(na, nb);
			}

			if (na == nb) {
				valid = (na > TYPE_BVEC4 && na <= TYPE_MAT4);
				ret_type = na;
			} else if (na == TYPE_INT && nb == TYPE_IVEC2) {
				valid = true;
				ret_type = TYPE_IVEC2;
			} else if (na == TYPE_INT && nb == TYPE_IVEC3) {
				valid = true;
				ret_type = TYPE_IVEC3;
			} else if (na == TYPE_INT && nb == TYPE_IVEC4) {
				valid = true;
				ret_type = TYPE_IVEC4;
			} else if (na == TYPE_UINT && nb == TYPE_UVEC2) {
				valid = true;
				ret_type = TYPE_UVEC2;
			} else if (na == TYPE_UINT && nb == TYPE_UVEC3) {
				valid = true;
				ret_type = TYPE_UVEC3;
			} else if (na == TYPE_UINT && nb == TYPE_UVEC4) {
				valid = true;
				ret_type = TYPE_UVEC4;
			} else if (na == TYPE_FLOAT && nb == TYPE_VEC2) {
				valid = true;
				ret_type = TYPE_VEC2;
			} else if (na == TYPE_FLOAT && nb == TYPE_VEC3) {
				valid = true;
				ret_type = TYPE_VEC3;
			} else if (na == TYPE_FLOAT && nb == TYPE_VEC4) {
				valid = true;
				ret_type = TYPE_VEC4;
			} else if (na == TYPE_FLOAT && nb == TYPE_MAT2) {
				valid = true;
				ret_type = TYPE_MAT2;
			} else if (na == TYPE_FLOAT && nb == TYPE_MAT3) {
				valid = true;
				ret_type = TYPE_MAT3;
			} else if (na == TYPE_FLOAT && nb == TYPE_MAT4) {
				valid = true;
				ret_type = TYPE_MAT4;
			} else if (p_op->op == OP_MUL && na == TYPE_VEC2 && nb == TYPE_MAT2) {
				valid = true;
				ret_type = TYPE_VEC2;
			} else if (p_op->op == OP_MUL && na == TYPE_VEC3 && nb == TYPE_MAT3) {
				valid = true;
				ret_type = TYPE_VEC3;
			} else if (p_op->op == OP_MUL && na == TYPE_VEC4 && nb == TYPE_MAT4) {
				valid = true;
				ret_type = TYPE_VEC4;
			}
		} break;
		case OP_ASSIGN_MOD:
		case OP_MOD: {
			/*
			 * The operator modulus (%) operates on signed or unsigned integers or integer vectors. The operand
			 * types must both be signed or both be unsigned. The operands cannot be vectors of differing size. If
			 * one operand is a scalar and the other vector, then the scalar is applied component-wise to the vector,
			 * resulting in the same type as the vector. If both are vectors of the same size, the result is computed
			 * component-wise.
			 */

			if ((!p_op->arguments[0]->is_indexed() && p_op->arguments[0]->get_array_size() > 0) || (!p_op->arguments[1]->is_indexed() && p_op->arguments[1]->get_array_size() > 0)) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();

			if (na == TYPE_INT && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_INT;
			} else if (na == TYPE_IVEC2 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC2;
			} else if (na == TYPE_IVEC3 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC3;
			} else if (na == TYPE_IVEC4 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC4;
			} else if (na == TYPE_IVEC2 && nb == TYPE_IVEC2) {
				valid = true;
				ret_type = TYPE_IVEC2;
			} else if (na == TYPE_IVEC3 && nb == TYPE_IVEC3) {
				valid = true;
				ret_type = TYPE_IVEC3;
			} else if (na == TYPE_IVEC4 && nb == TYPE_IVEC4) {
				valid = true;
				ret_type = TYPE_IVEC4;
				/////
			} else if (na == TYPE_UINT && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UINT;
			} else if (na == TYPE_UVEC2 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC2;
			} else if (na == TYPE_UVEC3 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC3;
			} else if (na == TYPE_UVEC4 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC4;
			} else if (na == TYPE_UVEC2 && nb == TYPE_UVEC2) {
				valid = true;
				ret_type = TYPE_UVEC2;
			} else if (na == TYPE_UVEC3 && nb == TYPE_UVEC3) {
				valid = true;
				ret_type = TYPE_UVEC3;
			} else if (na == TYPE_UVEC4 && nb == TYPE_UVEC4) {
				valid = true;
				ret_type = TYPE_UVEC4;
			}
		} break;
		case OP_ASSIGN_SHIFT_LEFT:
		case OP_ASSIGN_SHIFT_RIGHT:
		case OP_SHIFT_LEFT:
		case OP_SHIFT_RIGHT: {
			if ((!p_op->arguments[0]->is_indexed() && p_op->arguments[0]->get_array_size() > 0) || (!p_op->arguments[1]->is_indexed() && p_op->arguments[1]->get_array_size() > 0)) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();

			if (na == TYPE_INT && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_INT;
			} else if (na == TYPE_IVEC2 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC2;
			} else if (na == TYPE_IVEC3 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC3;
			} else if (na == TYPE_IVEC4 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC4;
			} else if (na == TYPE_IVEC2 && nb == TYPE_IVEC2) {
				valid = true;
				ret_type = TYPE_IVEC2;
			} else if (na == TYPE_IVEC3 && nb == TYPE_IVEC3) {
				valid = true;
				ret_type = TYPE_IVEC3;
			} else if (na == TYPE_IVEC4 && nb == TYPE_IVEC4) {
				valid = true;
				ret_type = TYPE_IVEC4;
			} else if (na == TYPE_UINT && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UINT;
			} else if (na == TYPE_UVEC2 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC2;
			} else if (na == TYPE_UVEC3 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC3;
			} else if (na == TYPE_UVEC4 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC4;
			} else if (na == TYPE_UVEC2 && nb == TYPE_UVEC2) {
				valid = true;
				ret_type = TYPE_UVEC2;
			} else if (na == TYPE_UVEC3 && nb == TYPE_UVEC3) {
				valid = true;
				ret_type = TYPE_UVEC3;
			} else if (na == TYPE_UVEC4 && nb == TYPE_UVEC4) {
				valid = true;
				ret_type = TYPE_UVEC4;
			}
		} break;
		case OP_ASSIGN: {
			int sa = 0;
			int sb = 0;
			if (!p_op->arguments[0]->is_indexed()) {
				sa = p_op->arguments[0]->get_array_size();
			}
			if (!p_op->arguments[1]->is_indexed()) {
				sb = p_op->arguments[1]->get_array_size();
			}
			if (sa != sb) {
				break; // don't accept arrays if their sizes are not equal
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();
			if (na == TYPE_STRUCT || nb == TYPE_STRUCT) {
				valid = p_op->arguments[0]->get_datatype_name() == p_op->arguments[1]->get_datatype_name();
			} else {
				valid = na == nb;
			}
			ret_type = na;
			ret_size = sa;
		} break;
		case OP_ASSIGN_ADD:
		case OP_ASSIGN_SUB:
		case OP_ASSIGN_MUL:
		case OP_ASSIGN_DIV: {
			int sa = 0;
			int sb = 0;
			if (!p_op->arguments[0]->is_indexed()) {
				sa = p_op->arguments[0]->get_array_size();
			}
			if (!p_op->arguments[1]->is_indexed()) {
				sb = p_op->arguments[1]->get_array_size();
			}
			if (sa > 0 || sb > 0) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();

			if (na == nb) {
				valid = (na > TYPE_BVEC4 && na <= TYPE_MAT4);
				ret_type = na;
			} else if (na == TYPE_IVEC2 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC2;
			} else if (na == TYPE_IVEC3 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC3;
			} else if (na == TYPE_IVEC4 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC4;
			} else if (na == TYPE_UVEC2 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC2;
			} else if (na == TYPE_UVEC3 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC3;
			} else if (na == TYPE_UVEC4 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC4;
			} else if (na == TYPE_VEC2 && nb == TYPE_FLOAT) {
				valid = true;
				ret_type = TYPE_VEC2;
			} else if (na == TYPE_VEC3 && nb == TYPE_FLOAT) {
				valid = true;
				ret_type = TYPE_VEC3;
			} else if (na == TYPE_VEC4 && nb == TYPE_FLOAT) {
				valid = true;
				ret_type = TYPE_VEC4;
			} else if (na == TYPE_MAT2 && nb == TYPE_FLOAT) {
				valid = true;
				ret_type = TYPE_MAT2;
			} else if (na == TYPE_MAT3 && nb == TYPE_FLOAT) {
				valid = true;
				ret_type = TYPE_MAT3;
			} else if (na == TYPE_MAT4 && nb == TYPE_FLOAT) {
				valid = true;
				ret_type = TYPE_MAT4;
			} else if (p_op->op == OP_ASSIGN_MUL && na == TYPE_VEC2 && nb == TYPE_MAT2) {
				valid = true;
				ret_type = TYPE_VEC2;
			} else if (p_op->op == OP_ASSIGN_MUL && na == TYPE_VEC3 && nb == TYPE_MAT3) {
				valid = true;
				ret_type = TYPE_VEC3;
			} else if (p_op->op == OP_ASSIGN_MUL && na == TYPE_VEC4 && nb == TYPE_MAT4) {
				valid = true;
				ret_type = TYPE_VEC4;
			}
		} break;
		case OP_ASSIGN_BIT_AND:
		case OP_ASSIGN_BIT_OR:
		case OP_ASSIGN_BIT_XOR:
		case OP_BIT_AND:
		case OP_BIT_OR:
		case OP_BIT_XOR: {
			/*
			 * The bitwise operators and (&), exclusive-or (^), and inclusive-or (|). The operands must be of type
			 * signed or unsigned integers or integer vectors. The operands cannot be vectors of differing size. If
			 * one operand is a scalar and the other a vector, the scalar is applied component-wise to the vector,
			 * resulting in the same type as the vector. The fundamental types of the operands (signed or unsigned)
			 * must match.
			 */

			int sa = 0;
			int sb = 0;
			if (!p_op->arguments[0]->is_indexed()) {
				sa = p_op->arguments[0]->get_array_size();
			}
			if (!p_op->arguments[1]->is_indexed()) {
				sb = p_op->arguments[1]->get_array_size();
			}
			if (sa > 0 || sb > 0) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();

			if (na > nb && p_op->op >= OP_BIT_AND) {
				//can swap for non assign
				SWAP(na, nb);
			}

			if (na == TYPE_INT && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_INT;
			} else if (na == TYPE_IVEC2 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC2;
			} else if (na == TYPE_IVEC3 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC3;
			} else if (na == TYPE_IVEC4 && nb == TYPE_INT) {
				valid = true;
				ret_type = TYPE_IVEC4;
			} else if (na == TYPE_IVEC2 && nb == TYPE_IVEC2) {
				valid = true;
				ret_type = TYPE_IVEC2;
			} else if (na == TYPE_IVEC3 && nb == TYPE_IVEC3) {
				valid = true;
				ret_type = TYPE_IVEC3;
			} else if (na == TYPE_IVEC4 && nb == TYPE_IVEC4) {
				valid = true;
				ret_type = TYPE_IVEC4;
				/////
			} else if (na == TYPE_UINT && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UINT;
			} else if (na == TYPE_UVEC2 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC2;
			} else if (na == TYPE_UVEC3 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC3;
			} else if (na == TYPE_UVEC4 && nb == TYPE_UINT) {
				valid = true;
				ret_type = TYPE_UVEC4;
			} else if (na == TYPE_UVEC2 && nb == TYPE_UVEC2) {
				valid = true;
				ret_type = TYPE_UVEC2;
			} else if (na == TYPE_UVEC3 && nb == TYPE_UVEC3) {
				valid = true;
				ret_type = TYPE_UVEC3;
			} else if (na == TYPE_UVEC4 && nb == TYPE_UVEC4) {
				valid = true;
				ret_type = TYPE_UVEC4;
			}
		} break;
		case OP_BIT_INVERT: { //unaries
			if (!p_op->arguments[0]->is_indexed() && p_op->arguments[0]->get_array_size() > 0) {
				break; // don't accept arrays
			}

			DataType na = p_op->arguments[0]->get_datatype();
			valid = na >= TYPE_INT && na < TYPE_FLOAT;
			ret_type = na;
		} break;
		case OP_SELECT_IF: {
			int sa = 0;
			int sb = 0;
			if (!p_op->arguments[1]->is_indexed()) {
				sa = p_op->arguments[1]->get_array_size();
			}
			if (!p_op->arguments[2]->is_indexed()) {
				sb = p_op->arguments[2]->get_array_size();
			}
			if (sa != sb) {
				break; // don't accept arrays if their sizes are not equal
			}

			DataType na = p_op->arguments[0]->get_datatype();
			DataType nb = p_op->arguments[1]->get_datatype();
			DataType nc = p_op->arguments[2]->get_datatype();

			valid = na == TYPE_BOOL && (nb == nc);
			ret_type = nb;
			ret_size = sa;
		} break;
		default: {
			ERR_FAIL_V(false);
		}
	}

	if (r_ret_type) {
		*r_ret_type = ret_type;
	}
	if (r_ret_size) {
		*r_ret_size = ret_size;
	}

	if (valid && (!p_block || p_block->use_op_eval)) {
		// Need to be placed here and not in the `_reduce_expression` because otherwise expressions like `1 + 2 / 2` will not work correctly.
		valid = _eval_operator(p_block, p_op);
	}

	return valid;
}

Vector<ShaderLanguage::Scalar> ShaderLanguage::_get_node_values(const BlockNode *p_block, Node *p_node) {
	Vector<Scalar> result;

	switch (p_node->type) {
		case Node::NODE_TYPE_VARIABLE: {
			_find_identifier(p_block, false, FunctionInfo(), static_cast<VariableNode *>(p_node)->name, nullptr, nullptr, nullptr, nullptr, nullptr, &result);
		} break;
		default: {
			result = p_node->get_values();
		} break;
	}

	return result;
}

bool ShaderLanguage::_eval_operator(const BlockNode *p_block, OperatorNode *p_op) {
	bool is_valid = true;

	switch (p_op->op) {
		case OP_EQUAL:
		case OP_NOT_EQUAL:
		case OP_LESS:
		case OP_LESS_EQUAL:
		case OP_GREATER:
		case OP_GREATER_EQUAL:
		case OP_AND:
		case OP_OR:
		case OP_ADD:
		case OP_SUB:
		case OP_MUL:
		case OP_DIV:
		case OP_MOD:
		case OP_SHIFT_LEFT:
		case OP_SHIFT_RIGHT:
		case OP_BIT_AND:
		case OP_BIT_OR:
		case OP_BIT_XOR: {
			DataType a = p_op->arguments[0]->get_datatype();
			DataType b = p_op->arguments[1]->get_datatype();

			bool is_op_vec_transform = false;
			if (p_op->op == OP_MUL) {
				DataType ta = a;
				DataType tb = b;

				if (ta > tb) {
					SWAP(ta, tb);
				}
				if (ta == TYPE_VEC2 && tb == TYPE_MAT2) {
					is_op_vec_transform = true;
				} else if (ta == TYPE_VEC3 && tb == TYPE_MAT3) {
					is_op_vec_transform = true;
				} else if (ta == TYPE_VEC4 && tb == TYPE_MAT4) {
					is_op_vec_transform = true;
				}
			}

			Vector<Scalar> va = _get_node_values(p_block, p_op->arguments[0]);
			Vector<Scalar> vb = _get_node_values(p_block, p_op->arguments[1]);

			if (is_op_vec_transform) {
				p_op->values = _eval_vector_transform(va, vb, a, b, p_op->get_datatype());
			} else {
				p_op->values = _eval_vector(va, vb, a, b, p_op->get_datatype(), p_op->op, is_valid);
			}
		} break;
		case OP_NOT:
		case OP_NEGATE:
		case OP_BIT_INVERT: {
			p_op->values = _eval_unary_vector(_get_node_values(p_block, p_op->arguments[0]), p_op->get_datatype(), p_op->op);
		} break;
		default: {
		} break;
	}

	return is_valid;
}

ShaderLanguage::Scalar ShaderLanguage::_eval_unary_scalar(const Scalar &p_a, Operator p_op, DataType p_ret_type) {
	Scalar scalar;

	switch (p_op) {
		case OP_NOT: {
			scalar.boolean = !p_a.boolean;
		} break;
		case OP_NEGATE: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = -p_a.sint;
			} else if (p_ret_type >= TYPE_UINT && p_ret_type <= TYPE_UVEC4) {
				// Intentionally wrap the unsigned int value, because GLSL does.
				scalar.uint = 0 - p_a.uint;
			} else { // float types
				scalar.real = -scalar.real;
			}
		} break;
		case OP_BIT_INVERT: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = ~p_a.sint;
			} else { // uint types
				scalar.uint = ~p_a.uint;
			}
		} break;
		default: {
		} break;
	}

	return scalar;
}

ShaderLanguage::Scalar ShaderLanguage::_eval_scalar(const Scalar &p_a, const Scalar &p_b, Operator p_op, DataType p_ret_type, bool &r_is_valid) {
	Scalar scalar;

	switch (p_op) {
		case OP_EQUAL: {
			scalar.boolean = p_a.boolean == p_b.boolean;
		} break;
		case OP_NOT_EQUAL: {
			scalar.boolean = p_a.boolean != p_b.boolean;
		} break;
		case OP_LESS: {
			if (p_ret_type == TYPE_INT) {
				scalar.boolean = p_a.sint < p_b.sint;
			} else if (p_ret_type == TYPE_UINT) {
				scalar.boolean = p_a.uint < p_b.uint;
			} else { // float type
				scalar.boolean = p_a.real < p_b.real;
			}
		} break;
		case OP_LESS_EQUAL: {
			if (p_ret_type == TYPE_INT) {
				scalar.boolean = p_a.sint <= p_b.sint;
			} else if (p_ret_type == TYPE_UINT) {
				scalar.boolean = p_a.uint <= p_b.uint;
			} else { // float type
				scalar.boolean = p_a.real <= p_b.real;
			}
		} break;
		case OP_GREATER: {
			if (p_ret_type == TYPE_INT) {
				scalar.boolean = p_a.sint > p_b.sint;
			} else if (p_ret_type == TYPE_UINT) {
				scalar.boolean = p_a.uint > p_b.uint;
			} else { // float type
				scalar.boolean = p_a.real > p_b.real;
			}
		} break;
		case OP_GREATER_EQUAL: {
			if (p_ret_type == TYPE_INT) {
				scalar.boolean = p_a.sint >= p_b.sint;
			} else if (p_ret_type == TYPE_UINT) {
				scalar.boolean = p_a.uint >= p_b.uint;
			} else { // float type
				scalar.boolean = p_a.real >= p_b.real;
			}
		} break;
		case OP_AND: {
			scalar.boolean = p_a.boolean && p_b.boolean;
		} break;
		case OP_OR: {
			scalar.boolean = p_a.boolean || p_b.boolean;
		} break;
		case OP_ADD: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = p_a.sint + p_b.sint;
			} else if (p_ret_type >= TYPE_UINT && p_ret_type <= TYPE_UVEC4) {
				scalar.uint = p_a.uint + p_b.uint;
			} else { // float + matrix types
				scalar.real = p_a.real + p_b.real;
			}
		} break;
		case OP_SUB: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = p_a.sint - p_b.sint;
			} else if (p_ret_type >= TYPE_UINT && p_ret_type <= TYPE_UVEC4) {
				scalar.uint = p_a.uint - p_b.uint;
			} else { // float + matrix types
				scalar.real = p_a.real - p_b.real;
			}
		} break;
		case OP_MUL: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = p_a.sint * p_b.sint;
			} else if (p_ret_type >= TYPE_UINT && p_ret_type <= TYPE_UVEC4) {
				scalar.uint = p_a.uint * p_b.uint;
			} else { // float + matrix types
				scalar.real = p_a.real * p_b.real;
			}
		} break;
		case OP_DIV: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				if (p_b.sint == 0) {
					_set_error(RTR("Division by zero error."));
					r_is_valid = false;
					break;
				}
				scalar.sint = p_a.sint / p_b.sint;
			} else if (p_ret_type == TYPE_UINT && p_ret_type <= TYPE_UVEC4) {
				if (p_b.uint == 0U) {
					_set_error(RTR("Division by zero error."));
					r_is_valid = false;
					break;
				}
				scalar.uint = p_a.uint / p_b.uint;
			} else { // float + matrix types
				scalar.real = p_a.real / p_b.real;
			}
		} break;
		case OP_MOD: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				if (p_b.sint == 0) {
					_set_error(RTR("Modulo by zero error."));
					r_is_valid = false;
					break;
				}
				scalar.sint = p_a.sint % p_b.sint;
			} else { // uint types
				if (p_b.uint == 0U) {
					_set_error(RTR("Modulo by zero error."));
					r_is_valid = false;
					break;
				}
				scalar.uint = p_a.uint % p_b.uint;
			}
		} break;
		case OP_SHIFT_LEFT: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = p_a.sint << p_b.sint;
			} else { // uint types
				scalar.uint = p_a.uint << p_b.uint;
			}
		} break;
		case OP_SHIFT_RIGHT: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = p_a.sint >> p_b.sint;
			} else { // uint types
				scalar.uint = p_a.uint >> p_b.uint;
			}
		} break;
		case OP_BIT_AND: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = p_a.sint & p_b.sint;
			} else { // uint types
				scalar.uint = p_a.uint & p_b.uint;
			}
		} break;
		case OP_BIT_OR: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = p_a.sint | p_b.sint;
			} else { // uint types
				scalar.uint = p_a.uint | p_b.uint;
			}
		} break;
		case OP_BIT_XOR: {
			if (p_ret_type >= TYPE_INT && p_ret_type <= TYPE_IVEC4) {
				scalar.sint = p_a.sint ^ p_b.sint;
			} else { // uint types
				scalar.uint = p_a.uint ^ p_b.uint;
			}
		} break;
		default: {
		} break;
	}

	return scalar;
}

Vector<ShaderLanguage::Scalar> ShaderLanguage::_eval_unary_vector(const Vector<Scalar> &p_va, DataType p_ret_type, Operator p_op) {
	uint32_t size = get_datatype_component_count(p_ret_type);
	if (p_va.size() != p_ret_type) {
		return Vector<Scalar>(); // Non-evaluable values should not be parsed further.
	}
	Vector<Scalar> value;
	value.resize(size);

	Scalar *w = value.ptrw();
	for (uint32_t i = 0U; i < size; i++) {
		w[i] = _eval_unary_scalar(p_va[i], p_op, p_ret_type);
	}
	return value;
}

Vector<ShaderLanguage::Scalar> ShaderLanguage::_eval_vector(const Vector<Scalar> &p_va, const Vector<Scalar> &p_vb, DataType p_left_type, DataType p_right_type, DataType p_ret_type, Operator p_op, bool &r_is_valid) {
	uint32_t left_size = get_datatype_component_count(p_left_type);
	uint32_t right_size = get_datatype_component_count(p_right_type);

	if (p_va.size() != left_size || p_vb.size() != right_size) {
		return Vector<Scalar>(); // Non-evaluable values should not be parsed further.
	}

	uint32_t ret_size = get_datatype_component_count(p_ret_type);
	Vector<Scalar> value;
	value.resize(ret_size);

	Scalar *w = value.ptrw();
	for (uint32_t i = 0U; i < ret_size; i++) {
		w[i] = _eval_scalar(p_va[MIN(i, left_size - 1)], p_vb[MIN(i, right_size - 1)], p_op, p_ret_type, r_is_valid);
		if (!r_is_valid) {
			return value;
		}
	}
	return value;
}

Vector<ShaderLanguage::Scalar> ShaderLanguage::_eval_vector_transform(const Vector<Scalar> &p_va, const Vector<Scalar> &p_vb, DataType p_left_type, DataType p_right_type, DataType p_ret_type) {
	uint32_t left_size = get_datatype_component_count(p_left_type);
	uint32_t right_size = get_datatype_component_count(p_right_type);

	if (p_va.size() != left_size || p_vb.size() != right_size) {
		return Vector<Scalar>(); // Non-evaluable values should not be parsed further.
	}

	uint32_t ret_size = get_datatype_component_count(p_ret_type);
	Vector<Scalar> value;
	value.resize_zeroed(ret_size);

	Scalar *w = value.ptrw();
	switch (p_ret_type) {
		case TYPE_VEC2: {
			if (left_size == 2) { // v * m
				Vector2 v = Vector2(p_va[0].real, p_va[1].real);

				w[0].real = (p_vb[0].real * v.x + p_vb[1].real * v.y);
				w[1].real = (p_vb[2].real * v.x + p_vb[3].real * v.y);
			} else { // m * v
				Vector2 v = Vector2(p_vb[0].real, p_vb[1].real);

				w[0].real = (p_va[0].real * v.x + p_va[2].real * v.y);
				w[1].real = (p_va[1].real * v.x + p_va[3].real * v.y);
			}
		} break;
		case TYPE_VEC3: {
			if (left_size == 3) { // v * m
				Vector3 v = Vector3(p_va[0].real, p_va[1].real, p_va[2].real);

				w[0].real = (p_vb[0].real * v.x + p_vb[1].real * v.y + p_vb[2].real * v.z);
				w[1].real = (p_vb[3].real * v.x + p_vb[4].real * v.y + p_vb[5].real * v.z);
				w[2].real = (p_vb[6].real * v.x + p_vb[7].real * v.y + p_vb[8].real * v.z);
			} else { // m * v
				Vector3 v = Vector3(p_vb[0].real, p_vb[1].real, p_vb[2].real);

				w[0].real = (p_va[0].real * v.x + p_va[3].real * v.y + p_va[6].real * v.z);
				w[1].real = (p_va[1].real * v.x + p_va[4].real * v.y + p_va[7].real * v.z);
				w[2].real = (p_va[2].real * v.x + p_va[5].real * v.y + p_va[8].real * v.z);
			}
		} break;
		case TYPE_VEC4: {
			if (left_size == 4) { // v * m
				Vector4 v = Vector4(p_va[0].real, p_va[1].real, p_va[2].real, p_va[3].real);

				w[0].real = (p_vb[0].real * v.x + p_vb[1].real * v.y + p_vb[2].real * v.z + p_vb[3].real * v.w);
				w[1].real = (p_vb[4].real * v.x + p_vb[5].real * v.y + p_vb[6].real * v.z + p_vb[7].real * v.w);
				w[2].real = (p_vb[8].real * v.x + p_vb[9].real * v.y + p_vb[10].real * v.z + p_vb[11].real * v.w);
				w[3].real = (p_vb[12].real * v.x + p_vb[13].real * v.y + p_vb[14].real * v.z + p_vb[15].real * v.w);
			} else { // m * v
				Vector4 v = Vector4(p_vb[0].real, p_vb[1].real, p_vb[2].real, p_vb[3].real);

				w[0].real = (p_vb[0].real * v.x + p_vb[4].real * v.y + p_vb[8].real * v.z + p_vb[12].real * v.w);
				w[1].real = (p_vb[1].real * v.x + p_vb[5].real * v.y + p_vb[9].real * v.z + p_vb[13].real * v.w);
				w[2].real = (p_vb[2].real * v.x + p_vb[6].real * v.y + p_vb[10].real * v.z + p_vb[14].real * v.w);
				w[3].real = (p_vb[3].real * v.x + p_vb[7].real * v.y + p_vb[11].real * v.z + p_vb[15].real * v.w);
			}
		} break;
		default: {
		} break;
	}

	return value;
}

const ShaderLanguage::BuiltinFuncDef ShaderLanguage::builtin_func_defs[] = {
	// Constructors.

	{ "bool", TYPE_BOOL, { TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec2", TYPE_BVEC2, { TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec2", TYPE_BVEC2, { TYPE_BOOL, TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec3", TYPE_BVEC3, { TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec3", TYPE_BVEC3, { TYPE_BOOL, TYPE_BOOL, TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec3", TYPE_BVEC3, { TYPE_BVEC2, TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec3", TYPE_BVEC3, { TYPE_BOOL, TYPE_BVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_BOOL, TYPE_BOOL, TYPE_BOOL, TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_BOOL, TYPE_BVEC2, TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_BVEC2, TYPE_BOOL, TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_BOOL, TYPE_BOOL, TYPE_BVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_BOOL, TYPE_BVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_BVEC3, TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_BVEC2, TYPE_BVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "float", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec2", TYPE_VEC2, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec2", TYPE_VEC2, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec3", TYPE_VEC3, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec3", TYPE_VEC3, { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec3", TYPE_VEC3, { TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec3", TYPE_VEC3, { TYPE_FLOAT, TYPE_VEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_FLOAT, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_VEC2, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_FLOAT, TYPE_VEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "int", TYPE_INT, { TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec2", TYPE_IVEC2, { TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec2", TYPE_IVEC2, { TYPE_INT, TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec3", TYPE_IVEC3, { TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec3", TYPE_IVEC3, { TYPE_INT, TYPE_INT, TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec3", TYPE_IVEC3, { TYPE_IVEC2, TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec3", TYPE_IVEC3, { TYPE_INT, TYPE_IVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_INT, TYPE_IVEC2, TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_IVEC2, TYPE_INT, TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_INT, TYPE_INT, TYPE_IVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_INT, TYPE_IVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_IVEC3, TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "uint", TYPE_UINT, { TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec2", TYPE_UVEC2, { TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec2", TYPE_UVEC2, { TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec3", TYPE_UVEC3, { TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec3", TYPE_UVEC3, { TYPE_UINT, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec3", TYPE_UVEC3, { TYPE_UVEC2, TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec3", TYPE_UVEC3, { TYPE_UINT, TYPE_UVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_UINT, TYPE_UINT, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_UINT, TYPE_UVEC2, TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_UVEC2, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_UINT, TYPE_UINT, TYPE_UVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_UINT, TYPE_UVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_UVEC3, TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "mat2", TYPE_MAT2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "mat3", TYPE_MAT3, { TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "mat4", TYPE_MAT4, { TYPE_VEC4, TYPE_VEC4, TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "mat2", TYPE_MAT2, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "mat3", TYPE_MAT3, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "mat4", TYPE_MAT4, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	// Conversion scalars.

	{ "int", TYPE_INT, { TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "int", TYPE_INT, { TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "int", TYPE_INT, { TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "int", TYPE_INT, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "float", TYPE_FLOAT, { TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "float", TYPE_FLOAT, { TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "float", TYPE_FLOAT, { TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "float", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "uint", TYPE_UINT, { TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uint", TYPE_UINT, { TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uint", TYPE_UINT, { TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uint", TYPE_UINT, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "bool", TYPE_BOOL, { TYPE_BOOL, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bool", TYPE_BOOL, { TYPE_INT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bool", TYPE_BOOL, { TYPE_UINT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bool", TYPE_BOOL, { TYPE_FLOAT, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	// Conversion vectors.

	{ "ivec2", TYPE_IVEC2, { TYPE_BVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec2", TYPE_IVEC2, { TYPE_IVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec2", TYPE_IVEC2, { TYPE_UVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec2", TYPE_IVEC2, { TYPE_VEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "vec2", TYPE_VEC2, { TYPE_BVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec2", TYPE_VEC2, { TYPE_IVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec2", TYPE_VEC2, { TYPE_UVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec2", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "uvec2", TYPE_UVEC2, { TYPE_BVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec2", TYPE_UVEC2, { TYPE_IVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec2", TYPE_UVEC2, { TYPE_UVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec2", TYPE_UVEC2, { TYPE_VEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "bvec2", TYPE_BVEC2, { TYPE_BVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec2", TYPE_BVEC2, { TYPE_IVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec2", TYPE_BVEC2, { TYPE_UVEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec2", TYPE_BVEC2, { TYPE_VEC2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "ivec3", TYPE_IVEC3, { TYPE_BVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec3", TYPE_IVEC3, { TYPE_IVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec3", TYPE_IVEC3, { TYPE_UVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec3", TYPE_IVEC3, { TYPE_VEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "vec3", TYPE_VEC3, { TYPE_BVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec3", TYPE_VEC3, { TYPE_IVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec3", TYPE_VEC3, { TYPE_UVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec3", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "uvec3", TYPE_UVEC3, { TYPE_BVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec3", TYPE_UVEC3, { TYPE_IVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec3", TYPE_UVEC3, { TYPE_UVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec3", TYPE_UVEC3, { TYPE_VEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "bvec3", TYPE_BVEC3, { TYPE_BVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec3", TYPE_BVEC3, { TYPE_IVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec3", TYPE_BVEC3, { TYPE_UVEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec3", TYPE_BVEC3, { TYPE_VEC3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "ivec4", TYPE_IVEC4, { TYPE_BVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_IVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_UVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "ivec4", TYPE_IVEC4, { TYPE_VEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "vec4", TYPE_VEC4, { TYPE_BVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_IVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_UVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "vec4", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "uvec4", TYPE_UVEC4, { TYPE_BVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_IVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_UVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "uvec4", TYPE_UVEC4, { TYPE_VEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	{ "bvec4", TYPE_BVEC4, { TYPE_BVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_IVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_UVEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "bvec4", TYPE_BVEC4, { TYPE_VEC4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	// Conversion between matrixes.

	{ "mat2", TYPE_MAT2, { TYPE_MAT3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "mat2", TYPE_MAT2, { TYPE_MAT4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "mat3", TYPE_MAT3, { TYPE_MAT2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "mat3", TYPE_MAT3, { TYPE_MAT4, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "mat4", TYPE_MAT4, { TYPE_MAT2, TYPE_VOID }, { "" }, TAG_GLOBAL, false },
	{ "mat4", TYPE_MAT4, { TYPE_MAT3, TYPE_VOID }, { "" }, TAG_GLOBAL, false },

	// Built-ins - trigonometric functions.
	// radians

	{ "radians", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "degrees" }, TAG_GLOBAL, false },
	{ "radians", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "degrees" }, TAG_GLOBAL, false },
	{ "radians", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "degrees" }, TAG_GLOBAL, false },
	{ "radians", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "degrees" }, TAG_GLOBAL, false },

	// degrees

	{ "degrees", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "radians" }, TAG_GLOBAL, false },
	{ "degrees", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "radians" }, TAG_GLOBAL, false },
	{ "degrees", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "radians" }, TAG_GLOBAL, false },
	{ "degrees", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "radians" }, TAG_GLOBAL, false },

	// sin

	{ "sin", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },
	{ "sin", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },
	{ "sin", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },
	{ "sin", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },

	// cos

	{ "cos", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },
	{ "cos", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },
	{ "cos", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },
	{ "cos", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },

	// tan

	{ "tan", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },
	{ "tan", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },
	{ "tan", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },
	{ "tan", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "angle" }, TAG_GLOBAL, false },

	// asin

	{ "asin", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "asin", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "asin", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "asin", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// acos

	{ "acos", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "acos", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "acos", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "acos", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// atan

	{ "atan", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "y_over_x" }, TAG_GLOBAL, false },
	{ "atan", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "y_over_x" }, TAG_GLOBAL, false },
	{ "atan", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "y_over_x" }, TAG_GLOBAL, false },
	{ "atan", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "y_over_x" }, TAG_GLOBAL, false },
	{ "atan", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "y", "x" }, TAG_GLOBAL, false },
	{ "atan", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "y", "x" }, TAG_GLOBAL, false },
	{ "atan", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "y", "x" }, TAG_GLOBAL, false },
	{ "atan", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "y", "x" }, TAG_GLOBAL, false },

	// sinh

	{ "sinh", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sinh", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sinh", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sinh", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// cosh

	{ "cosh", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "cosh", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "cosh", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "cosh", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// tanh

	{ "tanh", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "tanh", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "tanh", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "tanh", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// asinh

	{ "asinh", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "asinh", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "asinh", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "asinh", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// acosh

	{ "acosh", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "acosh", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "acosh", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "acosh", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// atanh

	{ "atanh", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "atanh", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "atanh", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "atanh", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// Builtins - exponential functions.
	// pow

	{ "pow", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },
	{ "pow", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },
	{ "pow", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },
	{ "pow", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },

	// exp

	{ "exp", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "exp", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "exp", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "exp", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// log

	{ "log", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "log", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "log", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "log", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// exp2

	{ "exp2", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "exp2", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "exp2", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "exp2", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// log2

	{ "log2", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "log2", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "log2", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "log2", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// sqrt

	{ "sqrt", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sqrt", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sqrt", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sqrt", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// inversesqrt

	{ "inversesqrt", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "inversesqrt", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "inversesqrt", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "inversesqrt", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// Built-ins - common functions.
	// abs

	{ "abs", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "abs", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "abs", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "abs", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	{ "abs", TYPE_INT, { TYPE_INT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "abs", TYPE_IVEC2, { TYPE_IVEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "abs", TYPE_IVEC3, { TYPE_IVEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "abs", TYPE_IVEC4, { TYPE_IVEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// sign

	{ "sign", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sign", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sign", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sign", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	{ "sign", TYPE_INT, { TYPE_INT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sign", TYPE_IVEC2, { TYPE_IVEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sign", TYPE_IVEC3, { TYPE_IVEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "sign", TYPE_IVEC4, { TYPE_IVEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// floor

	{ "floor", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "floor", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "floor", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "floor", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// trunc

	{ "trunc", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "trunc", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "trunc", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "trunc", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// round

	{ "round", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "round", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "round", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "round", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// roundEven

	{ "roundEven", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "roundEven", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "roundEven", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "roundEven", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// ceil

	{ "ceil", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "ceil", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "ceil", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "ceil", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// fract

	{ "fract", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "fract", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "fract", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "fract", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// mod

	{ "mod", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },
	{ "mod", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },
	{ "mod", TYPE_VEC2, { TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },
	{ "mod", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },
	{ "mod", TYPE_VEC3, { TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },
	{ "mod", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },
	{ "mod", TYPE_VEC4, { TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "x", "y" }, TAG_GLOBAL, false },

	// modf

	{ "modf", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "x", "i" }, TAG_GLOBAL, false },
	{ "modf", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "x", "i" }, TAG_GLOBAL, false },
	{ "modf", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "x", "i" }, TAG_GLOBAL, false },
	{ "modf", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "x", "i" }, TAG_GLOBAL, false },

	// min

	{ "min", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_VEC2, { TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_VEC3, { TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_VEC4, { TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "min", TYPE_INT, { TYPE_INT, TYPE_INT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_IVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_IVEC2, { TYPE_IVEC2, TYPE_INT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_IVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_IVEC3, { TYPE_IVEC3, TYPE_INT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_IVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_IVEC4, { TYPE_IVEC4, TYPE_INT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "min", TYPE_UINT, { TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_UVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_UVEC2, { TYPE_UVEC2, TYPE_UINT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_UVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_UVEC3, { TYPE_UVEC3, TYPE_UINT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_UVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "min", TYPE_UVEC4, { TYPE_UVEC4, TYPE_UINT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// max

	{ "max", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_VEC2, { TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_VEC3, { TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_VEC4, { TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "max", TYPE_INT, { TYPE_INT, TYPE_INT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_IVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_IVEC2, { TYPE_IVEC2, TYPE_INT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_IVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_IVEC3, { TYPE_IVEC3, TYPE_INT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_IVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_IVEC4, { TYPE_IVEC4, TYPE_INT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "max", TYPE_UINT, { TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_UVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_UVEC2, { TYPE_UVEC2, TYPE_UINT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_UVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_UVEC3, { TYPE_UVEC3, TYPE_UINT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_UVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "max", TYPE_UVEC4, { TYPE_UVEC4, TYPE_UINT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// clamp

	{ "clamp", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_VEC2, { TYPE_VEC2, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_VEC3, { TYPE_VEC3, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_VEC4, { TYPE_VEC4, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },

	{ "clamp", TYPE_INT, { TYPE_INT, TYPE_INT, TYPE_INT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_IVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_IVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_IVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_IVEC2, { TYPE_IVEC2, TYPE_INT, TYPE_INT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_IVEC3, { TYPE_IVEC3, TYPE_INT, TYPE_INT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_IVEC4, { TYPE_IVEC4, TYPE_INT, TYPE_INT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },

	{ "clamp", TYPE_UINT, { TYPE_UINT, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_UVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_UVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_UVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_UVEC2, { TYPE_UVEC2, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_UVEC3, { TYPE_UVEC3, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },
	{ "clamp", TYPE_UVEC4, { TYPE_UVEC4, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "x", "minVal", "maxVal" }, TAG_GLOBAL, false },

	// mix

	{ "mix", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },
	{ "mix", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },
	{ "mix", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_BVEC2, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },
	{ "mix", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },
	{ "mix", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },
	{ "mix", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_BVEC3, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },
	{ "mix", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },
	{ "mix", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },
	{ "mix", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_BVEC4, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },
	{ "mix", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b", "value" }, TAG_GLOBAL, false },

	// step

	{ "step", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "edge", "x" }, TAG_GLOBAL, false },
	{ "step", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "edge", "x" }, TAG_GLOBAL, false },
	{ "step", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "edge", "x" }, TAG_GLOBAL, false },
	{ "step", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "edge", "x" }, TAG_GLOBAL, false },
	{ "step", TYPE_VEC2, { TYPE_FLOAT, TYPE_VEC2, TYPE_VOID }, { "edge", "x" }, TAG_GLOBAL, false },
	{ "step", TYPE_VEC3, { TYPE_FLOAT, TYPE_VEC3, TYPE_VOID }, { "edge", "x" }, TAG_GLOBAL, false },
	{ "step", TYPE_VEC4, { TYPE_FLOAT, TYPE_VEC4, TYPE_VOID }, { "edge", "x" }, TAG_GLOBAL, false },

	// smoothstep

	{ "smoothstep", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "edge0", "edge1", "value" }, TAG_GLOBAL, false },
	{ "smoothstep", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "edge0", "edge1", "value" }, TAG_GLOBAL, false },
	{ "smoothstep", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "edge0", "edge1", "value" }, TAG_GLOBAL, false },
	{ "smoothstep", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "edge0", "edge1", "value" }, TAG_GLOBAL, false },
	{ "smoothstep", TYPE_VEC2, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VEC2, TYPE_VOID }, { "edge0", "edge1", "value" }, TAG_GLOBAL, false },
	{ "smoothstep", TYPE_VEC3, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VEC3, TYPE_VOID }, { "edge0", "edge1", "value" }, TAG_GLOBAL, false },
	{ "smoothstep", TYPE_VEC4, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VEC4, TYPE_VOID }, { "edge0", "edge1", "value" }, TAG_GLOBAL, false },

	// isnan

	{ "isnan", TYPE_BOOL, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "isnan", TYPE_BVEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "isnan", TYPE_BVEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "isnan", TYPE_BVEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// isinf

	{ "isinf", TYPE_BOOL, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "isinf", TYPE_BVEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "isinf", TYPE_BVEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "isinf", TYPE_BVEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// floatBitsToInt

	{ "floatBitsToInt", TYPE_INT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "floatBitsToInt", TYPE_IVEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "floatBitsToInt", TYPE_IVEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "floatBitsToInt", TYPE_IVEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// floatBitsToUint

	{ "floatBitsToUint", TYPE_UINT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "floatBitsToUint", TYPE_UVEC2, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "floatBitsToUint", TYPE_UVEC3, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "floatBitsToUint", TYPE_UVEC4, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// intBitsToFloat

	{ "intBitsToFloat", TYPE_FLOAT, { TYPE_INT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "intBitsToFloat", TYPE_VEC2, { TYPE_IVEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "intBitsToFloat", TYPE_VEC3, { TYPE_IVEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "intBitsToFloat", TYPE_VEC4, { TYPE_IVEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// uintBitsToFloat

	{ "uintBitsToFloat", TYPE_FLOAT, { TYPE_UINT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "uintBitsToFloat", TYPE_VEC2, { TYPE_UVEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "uintBitsToFloat", TYPE_VEC3, { TYPE_UVEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "uintBitsToFloat", TYPE_VEC4, { TYPE_UVEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// Built-ins - geometric functions.
	// length

	{ "length", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "length", TYPE_FLOAT, { TYPE_VEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "length", TYPE_FLOAT, { TYPE_VEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "length", TYPE_FLOAT, { TYPE_VEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// distance

	{ "distance", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "distance", TYPE_FLOAT, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "distance", TYPE_FLOAT, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "distance", TYPE_FLOAT, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// dot

	{ "dot", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "dot", TYPE_FLOAT, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "dot", TYPE_FLOAT, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "dot", TYPE_FLOAT, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// cross

	{ "cross", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// normalize

	{ "normalize", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "normalize", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "normalize", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "normalize", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },

	// reflect

	{ "reflect", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "I", "N" }, TAG_GLOBAL, false },
	{ "reflect", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "I", "N" }, TAG_GLOBAL, false },
	{ "reflect", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "I", "N" }, TAG_GLOBAL, false },

	// refract

	{ "refract", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "I", "N", "eta" }, TAG_GLOBAL, false },
	{ "refract", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "I", "N", "eta" }, TAG_GLOBAL, false },
	{ "refract", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "I", "N", "eta" }, TAG_GLOBAL, false },

	// faceforward

	{ "faceforward", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "N", "I", "Nref" }, TAG_GLOBAL, false },
	{ "faceforward", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "N", "I", "Nref" }, TAG_GLOBAL, false },
	{ "faceforward", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "N", "I", "Nref" }, TAG_GLOBAL, false },

	// matrixCompMult

	{ "matrixCompMult", TYPE_MAT2, { TYPE_MAT2, TYPE_MAT2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "matrixCompMult", TYPE_MAT3, { TYPE_MAT3, TYPE_MAT3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "matrixCompMult", TYPE_MAT4, { TYPE_MAT4, TYPE_MAT4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// outerProduct

	{ "outerProduct", TYPE_MAT2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "c", "r" }, TAG_GLOBAL, false },
	{ "outerProduct", TYPE_MAT3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "c", "r" }, TAG_GLOBAL, false },
	{ "outerProduct", TYPE_MAT4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "c", "r" }, TAG_GLOBAL, false },

	// transpose

	{ "transpose", TYPE_MAT2, { TYPE_MAT2, TYPE_VOID }, { "m" }, TAG_GLOBAL, false },
	{ "transpose", TYPE_MAT3, { TYPE_MAT3, TYPE_VOID }, { "m" }, TAG_GLOBAL, false },
	{ "transpose", TYPE_MAT4, { TYPE_MAT4, TYPE_VOID }, { "m" }, TAG_GLOBAL, false },

	// determinant

	{ "determinant", TYPE_FLOAT, { TYPE_MAT2, TYPE_VOID }, { "m" }, TAG_GLOBAL, false },
	{ "determinant", TYPE_FLOAT, { TYPE_MAT3, TYPE_VOID }, { "m" }, TAG_GLOBAL, false },
	{ "determinant", TYPE_FLOAT, { TYPE_MAT4, TYPE_VOID }, { "m" }, TAG_GLOBAL, false },

	// inverse

	{ "inverse", TYPE_MAT2, { TYPE_MAT2, TYPE_VOID }, { "m" }, TAG_GLOBAL, false },
	{ "inverse", TYPE_MAT3, { TYPE_MAT3, TYPE_VOID }, { "m" }, TAG_GLOBAL, false },
	{ "inverse", TYPE_MAT4, { TYPE_MAT4, TYPE_VOID }, { "m" }, TAG_GLOBAL, false },

	// lessThan

	{ "lessThan", TYPE_BVEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThan", TYPE_BVEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThan", TYPE_BVEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "lessThan", TYPE_BVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThan", TYPE_BVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThan", TYPE_BVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "lessThan", TYPE_BVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThan", TYPE_BVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThan", TYPE_BVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// greaterThan

	{ "greaterThan", TYPE_BVEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThan", TYPE_BVEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThan", TYPE_BVEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "greaterThan", TYPE_BVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThan", TYPE_BVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThan", TYPE_BVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "greaterThan", TYPE_BVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThan", TYPE_BVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThan", TYPE_BVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// lessThanEqual

	{ "lessThanEqual", TYPE_BVEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThanEqual", TYPE_BVEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThanEqual", TYPE_BVEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "lessThanEqual", TYPE_BVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThanEqual", TYPE_BVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThanEqual", TYPE_BVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "lessThanEqual", TYPE_BVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThanEqual", TYPE_BVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "lessThanEqual", TYPE_BVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// greaterThanEqual

	{ "greaterThanEqual", TYPE_BVEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThanEqual", TYPE_BVEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThanEqual", TYPE_BVEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "greaterThanEqual", TYPE_BVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThanEqual", TYPE_BVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThanEqual", TYPE_BVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "greaterThanEqual", TYPE_BVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThanEqual", TYPE_BVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "greaterThanEqual", TYPE_BVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// equal

	{ "equal", TYPE_BVEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "equal", TYPE_BVEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "equal", TYPE_BVEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "equal", TYPE_BVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "equal", TYPE_BVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "equal", TYPE_BVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "equal", TYPE_BVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "equal", TYPE_BVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "equal", TYPE_BVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "equal", TYPE_BVEC2, { TYPE_BVEC2, TYPE_BVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "equal", TYPE_BVEC3, { TYPE_BVEC3, TYPE_BVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "equal", TYPE_BVEC4, { TYPE_BVEC4, TYPE_BVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// notEqual

	{ "notEqual", TYPE_BVEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "notEqual", TYPE_BVEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "notEqual", TYPE_BVEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "notEqual", TYPE_BVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "notEqual", TYPE_BVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "notEqual", TYPE_BVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "notEqual", TYPE_BVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "notEqual", TYPE_BVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "notEqual", TYPE_BVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	{ "notEqual", TYPE_BVEC2, { TYPE_BVEC2, TYPE_BVEC2, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "notEqual", TYPE_BVEC3, { TYPE_BVEC3, TYPE_BVEC3, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },
	{ "notEqual", TYPE_BVEC4, { TYPE_BVEC4, TYPE_BVEC4, TYPE_VOID }, { "a", "b" }, TAG_GLOBAL, false },

	// any

	{ "any", TYPE_BOOL, { TYPE_BVEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "any", TYPE_BOOL, { TYPE_BVEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "any", TYPE_BOOL, { TYPE_BVEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// all

	{ "all", TYPE_BOOL, { TYPE_BVEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "all", TYPE_BOOL, { TYPE_BVEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "all", TYPE_BOOL, { TYPE_BVEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// not

	{ "not", TYPE_BVEC2, { TYPE_BVEC2, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "not", TYPE_BVEC3, { TYPE_BVEC3, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },
	{ "not", TYPE_BVEC4, { TYPE_BVEC4, TYPE_VOID }, { "x" }, TAG_GLOBAL, false },

	// Built-ins: texture functions.
	// textureSize

	{ "textureSize", TYPE_IVEC2, { TYPE_SAMPLER2D, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC2, { TYPE_ISAMPLER2D, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC2, { TYPE_USAMPLER2D, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC3, { TYPE_SAMPLER2DARRAY, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC3, { TYPE_ISAMPLER2DARRAY, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC3, { TYPE_USAMPLER2DARRAY, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC3, { TYPE_SAMPLER3D, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC3, { TYPE_ISAMPLER3D, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC3, { TYPE_USAMPLER3D, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC2, { TYPE_SAMPLERCUBE, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },
	{ "textureSize", TYPE_IVEC2, { TYPE_SAMPLERCUBEARRAY, TYPE_INT, TYPE_VOID }, { "sampler", "lod" }, TAG_GLOBAL, false },

	// texture

	{ "texture", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLER2DARRAY, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLER2DARRAY, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_UVEC4, { TYPE_USAMPLER2DARRAY, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_UVEC4, { TYPE_USAMPLER2DARRAY, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_IVEC4, { TYPE_ISAMPLER2DARRAY, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_IVEC4, { TYPE_ISAMPLER2DARRAY, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLER3D, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLER3D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_UVEC4, { TYPE_USAMPLER3D, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_UVEC4, { TYPE_USAMPLER3D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_IVEC4, { TYPE_ISAMPLER3D, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_IVEC4, { TYPE_ISAMPLER3D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLERCUBE, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLERCUBE, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLERCUBEARRAY, TYPE_VEC4, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLERCUBEARRAY, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLEREXT, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "texture", TYPE_VEC4, { TYPE_SAMPLEREXT, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },

	// textureProj

	{ "textureProj", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC4, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC4, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC4, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_VEC4, { TYPE_SAMPLER3D, TYPE_VEC4, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_VEC4, { TYPE_SAMPLER3D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_IVEC4, { TYPE_ISAMPLER3D, TYPE_VEC4, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_IVEC4, { TYPE_ISAMPLER3D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_UVEC4, { TYPE_USAMPLER3D, TYPE_VEC4, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureProj", TYPE_UVEC4, { TYPE_USAMPLER3D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "bias" }, TAG_GLOBAL, false },

	// textureLod

	{ "textureLod", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC2, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_VEC4, { TYPE_SAMPLER2DARRAY, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_IVEC4, { TYPE_ISAMPLER2DARRAY, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_UVEC4, { TYPE_USAMPLER2DARRAY, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_VEC4, { TYPE_SAMPLER3D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_IVEC4, { TYPE_ISAMPLER3D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_UVEC4, { TYPE_USAMPLER3D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_VEC4, { TYPE_SAMPLERCUBE, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureLod", TYPE_VEC4, { TYPE_SAMPLERCUBEARRAY, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },

	// texelFetch

	{ "texelFetch", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_IVEC2, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "texelFetch", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_IVEC2, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "texelFetch", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_IVEC2, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "texelFetch", TYPE_VEC4, { TYPE_SAMPLER2DARRAY, TYPE_IVEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "texelFetch", TYPE_IVEC4, { TYPE_ISAMPLER2DARRAY, TYPE_IVEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "texelFetch", TYPE_UVEC4, { TYPE_USAMPLER2DARRAY, TYPE_IVEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "texelFetch", TYPE_VEC4, { TYPE_SAMPLER3D, TYPE_IVEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "texelFetch", TYPE_IVEC4, { TYPE_ISAMPLER3D, TYPE_IVEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "texelFetch", TYPE_UVEC4, { TYPE_USAMPLER3D, TYPE_IVEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },

	// textureProjLod

	{ "textureProjLod", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureProjLod", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureProjLod", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureProjLod", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureProjLod", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC3, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureProjLod", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureProjLod", TYPE_VEC4, { TYPE_SAMPLER3D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureProjLod", TYPE_IVEC4, { TYPE_ISAMPLER3D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },
	{ "textureProjLod", TYPE_UVEC4, { TYPE_USAMPLER3D, TYPE_VEC4, TYPE_FLOAT, TYPE_VOID }, { "sampler", "coords", "lod" }, TAG_GLOBAL, false },

	// textureGrad

	{ "textureGrad", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC2, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC2, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC2, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_VEC4, { TYPE_SAMPLER2DARRAY, TYPE_VEC3, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_IVEC4, { TYPE_ISAMPLER2DARRAY, TYPE_VEC3, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_UVEC4, { TYPE_USAMPLER2DARRAY, TYPE_VEC3, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_VEC4, { TYPE_SAMPLER3D, TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_IVEC4, { TYPE_ISAMPLER3D, TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_UVEC4, { TYPE_USAMPLER3D, TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_VEC4, { TYPE_SAMPLERCUBE, TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureGrad", TYPE_VEC4, { TYPE_SAMPLERCUBEARRAY, TYPE_VEC4, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },

	// textureProjGrad

	{ "textureProjGrad", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC3, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureProjGrad", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC4, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureProjGrad", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC3, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureProjGrad", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC4, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureProjGrad", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC3, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureProjGrad", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC4, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureProjGrad", TYPE_VEC4, { TYPE_SAMPLER3D, TYPE_VEC4, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureProjGrad", TYPE_IVEC4, { TYPE_ISAMPLER3D, TYPE_VEC4, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },
	{ "textureProjGrad", TYPE_UVEC4, { TYPE_USAMPLER3D, TYPE_VEC4, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords", "dPdx", "dPdy" }, TAG_GLOBAL, false },

	// textureGather

	{ "textureGather", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC2, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_VEC4, { TYPE_SAMPLER2D, TYPE_VEC2, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "comp" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_IVEC4, { TYPE_ISAMPLER2D, TYPE_VEC2, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "comp" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_UVEC4, { TYPE_USAMPLER2D, TYPE_VEC2, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "comp" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_VEC4, { TYPE_SAMPLER2DARRAY, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_IVEC4, { TYPE_ISAMPLER2DARRAY, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_UVEC4, { TYPE_USAMPLER2DARRAY, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_VEC4, { TYPE_SAMPLER2DARRAY, TYPE_VEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "comp" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_IVEC4, { TYPE_ISAMPLER2DARRAY, TYPE_VEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "comp" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_UVEC4, { TYPE_USAMPLER2DARRAY, TYPE_VEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "comp" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_VEC4, { TYPE_SAMPLERCUBE, TYPE_VEC3, TYPE_VOID }, { "sampler", "coords" }, TAG_GLOBAL, false },
	{ "textureGather", TYPE_VEC4, { TYPE_SAMPLERCUBE, TYPE_VEC3, TYPE_INT, TYPE_VOID }, { "sampler", "coords", "comp" }, TAG_GLOBAL, false },

	// textureQueryLod

	{ "textureQueryLod", TYPE_VEC2, { TYPE_SAMPLER2D, TYPE_VEC2 }, { "sampler", "coords" }, TAG_GLOBAL, true },
	{ "textureQueryLod", TYPE_VEC2, { TYPE_ISAMPLER2D, TYPE_VEC2 }, { "sampler", "coords" }, TAG_GLOBAL, true },
	{ "textureQueryLod", TYPE_VEC2, { TYPE_USAMPLER2D, TYPE_VEC2 }, { "sampler", "coords" }, TAG_GLOBAL, true },
	{ "textureQueryLod", TYPE_VEC2, { TYPE_SAMPLER2DARRAY, TYPE_VEC2 }, { "sampler", "coords" }, TAG_GLOBAL, true },
	{ "textureQueryLod", TYPE_VEC2, { TYPE_ISAMPLER2DARRAY, TYPE_VEC2 }, { "sampler", "coords" }, TAG_GLOBAL, true },
	{ "textureQueryLod", TYPE_VEC2, { TYPE_USAMPLER2DARRAY, TYPE_VEC2 }, { "sampler", "coords" }, TAG_GLOBAL, true },
	{ "textureQueryLod", TYPE_VEC2, { TYPE_SAMPLER3D, TYPE_VEC3 }, { "sampler", "coords" }, TAG_GLOBAL, true },
	{ "textureQueryLod", TYPE_VEC2, { TYPE_ISAMPLER3D, TYPE_VEC3 }, { "sampler", "coords" }, TAG_GLOBAL, true },
	{ "textureQueryLod", TYPE_VEC2, { TYPE_USAMPLER3D, TYPE_VEC3 }, { "sampler", "coords" }, TAG_GLOBAL, true },
	{ "textureQueryLod", TYPE_VEC2, { TYPE_SAMPLERCUBE, TYPE_VEC3 }, { "sampler", "coords" }, TAG_GLOBAL, true },

	// textureQueryLevels

	{ "textureQueryLevels", TYPE_INT, { TYPE_SAMPLER2D }, { "sampler" }, TAG_GLOBAL, true },
	{ "textureQueryLevels", TYPE_INT, { TYPE_ISAMPLER2D }, { "sampler" }, TAG_GLOBAL, true },
	{ "textureQueryLevels", TYPE_INT, { TYPE_USAMPLER2D }, { "sampler" }, TAG_GLOBAL, true },
	{ "textureQueryLevels", TYPE_INT, { TYPE_SAMPLER2DARRAY }, { "sampler" }, TAG_GLOBAL, true },
	{ "textureQueryLevels", TYPE_INT, { TYPE_ISAMPLER2DARRAY }, { "sampler" }, TAG_GLOBAL, true },
	{ "textureQueryLevels", TYPE_INT, { TYPE_USAMPLER2DARRAY }, { "sampler" }, TAG_GLOBAL, true },
	{ "textureQueryLevels", TYPE_INT, { TYPE_SAMPLER3D }, { "sampler" }, TAG_GLOBAL, true },
	{ "textureQueryLevels", TYPE_INT, { TYPE_ISAMPLER3D }, { "sampler" }, TAG_GLOBAL, true },
	{ "textureQueryLevels", TYPE_INT, { TYPE_USAMPLER3D }, { "sampler" }, TAG_GLOBAL, true },
	{ "textureQueryLevels", TYPE_INT, { TYPE_SAMPLERCUBE }, { "sampler" }, TAG_GLOBAL, true },

	// dFdx

	{ "dFdx", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },
	{ "dFdx", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },
	{ "dFdx", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },
	{ "dFdx", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },

	// dFdxCoarse

	{ "dFdxCoarse", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdxCoarse", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdxCoarse", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdxCoarse", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },

	// dFdxFine

	{ "dFdxFine", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdxFine", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdxFine", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdxFine", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },

	// dFdy

	{ "dFdy", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },
	{ "dFdy", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },
	{ "dFdy", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },
	{ "dFdy", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },

	// dFdyCoarse

	{ "dFdyCoarse", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdyCoarse", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdyCoarse", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdyCoarse", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },

	// dFdyFine

	{ "dFdyFine", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdyFine", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdyFine", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "dFdyFine", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },

	// fwidth

	{ "fwidth", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },
	{ "fwidth", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },
	{ "fwidth", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },
	{ "fwidth", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "p" }, TAG_GLOBAL, false },

	// fwidthCoarse

	{ "fwidthCoarse", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "fwidthCoarse", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "fwidthCoarse", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "fwidthCoarse", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },

	// fwidthFine

	{ "fwidthFine", TYPE_FLOAT, { TYPE_FLOAT, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "fwidthFine", TYPE_VEC2, { TYPE_VEC2, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "fwidthFine", TYPE_VEC3, { TYPE_VEC3, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },
	{ "fwidthFine", TYPE_VEC4, { TYPE_VEC4, TYPE_VOID }, { "p" }, TAG_GLOBAL, true },

	// Sub-functions.
	// array

	{ "length", TYPE_INT, { TYPE_VOID }, { "" }, TAG_ARRAY, false },

	// Modern functions.
	// fma

	{ "fma", TYPE_FLOAT, { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT, TYPE_VOID }, { "a", "b", "c" }, TAG_GLOBAL, true },
	{ "fma", TYPE_VEC2, { TYPE_VEC2, TYPE_VEC2, TYPE_VEC2, TYPE_VOID }, { "a", "b", "c" }, TAG_GLOBAL, true },
	{ "fma", TYPE_VEC3, { TYPE_VEC3, TYPE_VEC3, TYPE_VEC3, TYPE_VOID }, { "a", "b", "c" }, TAG_GLOBAL, true },
	{ "fma", TYPE_VEC4, { TYPE_VEC4, TYPE_VEC4, TYPE_VEC4, TYPE_VOID }, { "a", "b", "c" }, TAG_GLOBAL, true },

	// Packing/Unpacking functions.

	{ "packHalf2x16", TYPE_UINT, { TYPE_VEC2, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "packUnorm2x16", TYPE_UINT, { TYPE_VEC2, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "packSnorm2x16", TYPE_UINT, { TYPE_VEC2, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "packUnorm4x8", TYPE_UINT, { TYPE_VEC4, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "packSnorm4x8", TYPE_UINT, { TYPE_VEC4, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },

	{ "unpackHalf2x16", TYPE_VEC2, { TYPE_UINT, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "unpackUnorm2x16", TYPE_VEC2, { TYPE_UINT, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "unpackSnorm2x16", TYPE_VEC2, { TYPE_UINT, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "unpackUnorm4x8", TYPE_VEC4, { TYPE_UINT, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },
	{ "unpackSnorm4x8", TYPE_VEC4, { TYPE_UINT, TYPE_VOID }, { "v" }, TAG_GLOBAL, false },

	// bitfieldExtract

	{ "bitfieldExtract", TYPE_INT, { TYPE_INT, TYPE_INT, TYPE_INT, TYPE_VOID }, { "value", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldExtract", TYPE_IVEC2, { TYPE_IVEC2, TYPE_INT, TYPE_INT, TYPE_VOID }, { "value", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldExtract", TYPE_IVEC3, { TYPE_IVEC3, TYPE_INT, TYPE_INT, TYPE_VOID }, { "value", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldExtract", TYPE_IVEC4, { TYPE_IVEC4, TYPE_INT, TYPE_INT, TYPE_VOID }, { "value", "offset", "bits" }, TAG_GLOBAL, true },

	{ "bitfieldExtract", TYPE_UINT, { TYPE_UINT, TYPE_INT, TYPE_INT, TYPE_VOID }, { "value", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldExtract", TYPE_UVEC2, { TYPE_UVEC2, TYPE_INT, TYPE_INT, TYPE_VOID }, { "value", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldExtract", TYPE_UVEC3, { TYPE_UVEC3, TYPE_INT, TYPE_INT, TYPE_VOID }, { "value", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldExtract", TYPE_UVEC4, { TYPE_UVEC4, TYPE_INT, TYPE_INT, TYPE_VOID }, { "value", "offset", "bits" }, TAG_GLOBAL, true },

	// bitfieldInsert

	{ "bitfieldInsert", TYPE_INT, { TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_VOID }, { "base", "insert", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldInsert", TYPE_IVEC2, { TYPE_IVEC2, TYPE_IVEC2, TYPE_INT, TYPE_INT, TYPE_VOID }, { "base", "insert", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldInsert", TYPE_IVEC3, { TYPE_IVEC3, TYPE_IVEC3, TYPE_INT, TYPE_INT, TYPE_VOID }, { "base", "insert", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldInsert", TYPE_IVEC4, { TYPE_IVEC4, TYPE_IVEC4, TYPE_INT, TYPE_INT, TYPE_VOID }, { "base", "insert", "offset", "bits" }, TAG_GLOBAL, true },

	{ "bitfieldInsert", TYPE_UINT, { TYPE_UINT, TYPE_UINT, TYPE_INT, TYPE_INT, TYPE_VOID }, { "base", "insert", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldInsert", TYPE_UVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_INT, TYPE_INT, TYPE_VOID }, { "base", "insert", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldInsert", TYPE_UVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_INT, TYPE_INT, TYPE_VOID }, { "base", "insert", "offset", "bits" }, TAG_GLOBAL, true },
	{ "bitfieldInsert", TYPE_UVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_INT, TYPE_INT, TYPE_VOID }, { "base", "insert", "offset", "bits" }, TAG_GLOBAL, true },

	// bitfieldReverse

	{ "bitfieldReverse", TYPE_INT, { TYPE_INT, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitfieldReverse", TYPE_IVEC2, { TYPE_IVEC2, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitfieldReverse", TYPE_IVEC3, { TYPE_IVEC3, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitfieldReverse", TYPE_IVEC4, { TYPE_IVEC4, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },

	{ "bitfieldReverse", TYPE_UINT, { TYPE_UINT, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitfieldReverse", TYPE_UVEC2, { TYPE_UVEC2, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitfieldReverse", TYPE_UVEC3, { TYPE_UVEC3, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitfieldReverse", TYPE_UVEC4, { TYPE_UVEC4, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },

	// bitCount

	{ "bitCount", TYPE_INT, { TYPE_INT, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitCount", TYPE_IVEC2, { TYPE_IVEC2, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitCount", TYPE_IVEC3, { TYPE_IVEC3, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitCount", TYPE_IVEC4, { TYPE_IVEC4, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },

	{ "bitCount", TYPE_UINT, { TYPE_UINT, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitCount", TYPE_UVEC2, { TYPE_UVEC2, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitCount", TYPE_UVEC3, { TYPE_UVEC3, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "bitCount", TYPE_UVEC4, { TYPE_UVEC4, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },

	// findLSB

	{ "findLSB", TYPE_INT, { TYPE_INT, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findLSB", TYPE_IVEC2, { TYPE_IVEC2, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findLSB", TYPE_IVEC3, { TYPE_IVEC3, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findLSB", TYPE_IVEC4, { TYPE_IVEC4, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },

	{ "findLSB", TYPE_UINT, { TYPE_UINT, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findLSB", TYPE_UVEC2, { TYPE_UVEC2, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findLSB", TYPE_UVEC3, { TYPE_UVEC3, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findLSB", TYPE_UVEC4, { TYPE_UVEC4, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },

	// findMSB

	{ "findMSB", TYPE_INT, { TYPE_INT, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findMSB", TYPE_IVEC2, { TYPE_IVEC2, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findMSB", TYPE_IVEC3, { TYPE_IVEC3, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findMSB", TYPE_IVEC4, { TYPE_IVEC4, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },

	{ "findMSB", TYPE_UINT, { TYPE_UINT, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findMSB", TYPE_UVEC2, { TYPE_UVEC2, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findMSB", TYPE_UVEC3, { TYPE_UVEC3, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },
	{ "findMSB", TYPE_UVEC4, { TYPE_UVEC4, TYPE_VOID }, { "value" }, TAG_GLOBAL, true },

	// umulExtended

	{ "umulExtended", TYPE_VOID, { TYPE_UINT, TYPE_UINT, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "x", "y", "msb", "lsb" }, TAG_GLOBAL, true },
	{ "umulExtended", TYPE_VOID, { TYPE_UVEC2, TYPE_UVEC2, TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "x", "y", "msb", "lsb" }, TAG_GLOBAL, true },
	{ "umulExtended", TYPE_VOID, { TYPE_UVEC3, TYPE_UVEC3, TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "x", "y", "msb", "lsb" }, TAG_GLOBAL, true },
	{ "umulExtended", TYPE_VOID, { TYPE_UVEC4, TYPE_UVEC4, TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "x", "y", "msb", "lsb" }, TAG_GLOBAL, true },

	// imulExtended

	{ "imulExtended", TYPE_VOID, { TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_VOID }, { "x", "y", "msb", "lsb" }, TAG_GLOBAL, true },
	{ "imulExtended", TYPE_VOID, { TYPE_IVEC2, TYPE_IVEC2, TYPE_IVEC2, TYPE_IVEC2, TYPE_VOID }, { "x", "y", "msb", "lsb" }, TAG_GLOBAL, true },
	{ "imulExtended", TYPE_VOID, { TYPE_IVEC3, TYPE_IVEC3, TYPE_IVEC3, TYPE_IVEC3, TYPE_VOID }, { "x", "y", "msb", "lsb" }, TAG_GLOBAL, true },
	{ "imulExtended", TYPE_VOID, { TYPE_IVEC4, TYPE_IVEC4, TYPE_IVEC4, TYPE_IVEC4, TYPE_VOID }, { "x", "y", "msb", "lsb" }, TAG_GLOBAL, true },

	// uaddCarry

	{ "uaddCarry", TYPE_UINT, { TYPE_UINT, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "x", "y", "carry" }, TAG_GLOBAL, true },
	{ "uaddCarry", TYPE_UVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "x", "y", "carry" }, TAG_GLOBAL, true },
	{ "uaddCarry", TYPE_UVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "x", "y", "carry" }, TAG_GLOBAL, true },
	{ "uaddCarry", TYPE_UVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "x", "y", "carry" }, TAG_GLOBAL, true },

	// usubBorrow

	{ "usubBorrow", TYPE_UINT, { TYPE_UINT, TYPE_UINT, TYPE_UINT, TYPE_VOID }, { "x", "y", "borrow" }, TAG_GLOBAL, true },
	{ "usubBorrow", TYPE_UVEC2, { TYPE_UVEC2, TYPE_UVEC2, TYPE_UVEC2, TYPE_VOID }, { "x", "y", "borrow" }, TAG_GLOBAL, true },
	{ "usubBorrow", TYPE_UVEC3, { TYPE_UVEC3, TYPE_UVEC3, TYPE_UVEC3, TYPE_VOID }, { "x", "y", "borrow" }, TAG_GLOBAL, true },
	{ "usubBorrow", TYPE_UVEC4, { TYPE_UVEC4, TYPE_UVEC4, TYPE_UVEC4, TYPE_VOID }, { "x", "y", "borrow" }, TAG_GLOBAL, true },

	// ldexp

	{ "ldexp", TYPE_FLOAT, { TYPE_FLOAT, TYPE_INT, TYPE_VOID }, { "x", "exp" }, TAG_GLOBAL, true },
	{ "ldexp", TYPE_VEC2, { TYPE_VEC2, TYPE_IVEC2, TYPE_VOID }, { "x", "exp" }, TAG_GLOBAL, true },
	{ "ldexp", TYPE_VEC3, { TYPE_VEC3, TYPE_IVEC3, TYPE_VOID }, { "x", "exp" }, TAG_GLOBAL, true },
	{ "ldexp", TYPE_VEC4, { TYPE_VEC4, TYPE_IVEC4, TYPE_VOID }, { "x", "exp" }, TAG_GLOBAL, true },

	// frexp

	{ "frexp", TYPE_FLOAT, { TYPE_FLOAT, TYPE_INT, TYPE_VOID }, { "x", "exp" }, TAG_GLOBAL, true },
	{ "frexp", TYPE_VEC2, { TYPE_VEC2, TYPE_IVEC2, TYPE_VOID }, { "x", "exp" }, TAG_GLOBAL, true },
	{ "frexp", TYPE_VEC3, { TYPE_VEC3, TYPE_IVEC3, TYPE_VOID }, { "x", "exp" }, TAG_GLOBAL, true },
	{ "frexp", TYPE_VEC4, { TYPE_VEC4, TYPE_IVEC4, TYPE_VOID }, { "x", "exp" }, TAG_GLOBAL, true },

	{ nullptr, TYPE_VOID, { TYPE_VOID }, { "" }, TAG_GLOBAL, false }
};

HashSet<StringName> global_func_set;

const ShaderLanguage::BuiltinFuncOutArgs ShaderLanguage::builtin_func_out_args[] = {
	{ "modf", { 1, -1 } },
	{ "umulExtended", { 2, 3 } },
	{ "imulExtended", { 2, 3 } },
	{ "uaddCarry", { 2, -1 } },
	{ "usubBorrow", { 2, -1 } },
	{ "ldexp", { 1, -1 } },
	{ "frexp", { 1, -1 } },
	{ nullptr, { 0, -1 } }
};

const ShaderLanguage::BuiltinFuncConstArgs ShaderLanguage::builtin_func_const_args[] = {
	{ "textureGather", 2, 0, 3 },
	{ nullptr, 0, 0, 0 }
};

const ShaderLanguage::BuiltinEntry ShaderLanguage::frag_only_func_defs[] = {
	{ "dFdx" },
	{ "dFdxCoarse" },
	{ "dFdxFine" },
	{ "dFdy" },
	{ "dFdyCoarse" },
	{ "dFdyFine" },
	{ "fwidth" },
	{ "fwidthCoarse" },
	{ "fwidthFine" },
	{ nullptr }
};

bool ShaderLanguage::is_const_suffix_lut_initialized = false;

bool ShaderLanguage::_validate_function_call(BlockNode *p_block, const FunctionInfo &p_function_info, OperatorNode *p_func, DataType *r_ret_type, StringName *r_ret_type_str, bool *r_is_custom_function) {
	ERR_FAIL_COND_V(p_func->op != OP_CALL && p_func->op != OP_CONSTRUCT, false);

	Vector<DataType> args;
	Vector<StringName> args2;
	Vector<int> args3;

	ERR_FAIL_COND_V(p_func->arguments[0]->type != Node::NODE_TYPE_VARIABLE, false);

	StringName name = static_cast<VariableNode *>(p_func->arguments[0])->name.operator String();
	StringName rname = static_cast<VariableNode *>(p_func->arguments[0])->rname.operator String();

	for (int i = 1; i < p_func->arguments.size(); i++) {
		args.push_back(p_func->arguments[i]->get_datatype());
		args2.push_back(p_func->arguments[i]->get_datatype_name());
		args3.push_back(p_func->arguments[i]->get_array_size());
	}

	int argcount = args.size();

	if (stages) {
		// Stage functions can be used in custom functions as well, that why need to check them all.
		for (const KeyValue<StringName, FunctionInfo> &E : *stages) {
			if (E.value.stage_functions.has(name)) {
				// Stage-based function.
				const StageFunctionInfo &sf = E.value.stage_functions[name];
				if (argcount != sf.arguments.size()) {
					_set_error(vformat(RTR("Invalid number of arguments when calling stage function '%s', which expects %d arguments."), String(name), sf.arguments.size()));
					return false;
				}
				// Validate arguments.
				for (int i = 0; i < argcount; i++) {
					if (args[i] != sf.arguments[i].type) {
						_set_error(vformat(RTR("Invalid argument type when calling stage function '%s', type expected is '%s'."), String(name), get_datatype_name(sf.arguments[i].type)));
						return false;
					}
				}

				if (r_ret_type) {
					*r_ret_type = sf.return_type;
				}
				if (r_ret_type_str) {
					*r_ret_type_str = "";
				}
				return true;
			}
		}
	}

	bool failed_builtin = false;
	bool unsupported_builtin = false;
	int builtin_idx = 0;

	if (argcount <= 4) {
		// test builtins
		int idx = 0;

		while (builtin_func_defs[idx].name) {
			if (completion_class != builtin_func_defs[idx].tag) {
				idx++;
				continue;
			}

			if (name == builtin_func_defs[idx].name) {
				failed_builtin = true;
				bool fail = false;
				for (int i = 0; i < argcount; i++) {
					if (p_func->arguments[i + 1]->type == Node::NODE_TYPE_ARRAY) {
						const ArrayNode *anode = static_cast<const ArrayNode *>(p_func->arguments[i + 1]);
						if (anode->call_expression == nullptr && !anode->is_indexed()) {
							fail = true;
							break;
						}
					}
					if (get_scalar_type(args[i]) == args[i] && p_func->arguments[i + 1]->type == Node::NODE_TYPE_CONSTANT && convert_constant(static_cast<ConstantNode *>(p_func->arguments[i + 1]), builtin_func_defs[idx].args[i])) {
						//all good, but needs implicit conversion later
					} else if (args[i] != builtin_func_defs[idx].args[i]) {
						fail = true;
						break;
					}
				}

				if (!fail) {
					if (RenderingServer::get_singleton()->is_low_end()) {
						if (builtin_func_defs[idx].high_end) {
							fail = true;
							unsupported_builtin = true;
							builtin_idx = idx;
						}
					}
				}

				if (!fail && argcount < 4 && builtin_func_defs[idx].args[argcount] != TYPE_VOID) {
					fail = true; //make sure the number of arguments matches
				}

				if (!fail) {
					{
						int constarg_idx = 0;
						while (builtin_func_const_args[constarg_idx].name) {
							if (String(name) == builtin_func_const_args[constarg_idx].name) {
								int arg = builtin_func_const_args[constarg_idx].arg + 1;
								if (p_func->arguments.size() <= arg) {
									break;
								}

								int min = builtin_func_const_args[constarg_idx].min;
								int max = builtin_func_const_args[constarg_idx].max;

								bool error = false;
								Vector<Scalar> values = _get_node_values(p_block, p_func->arguments[arg]);
								if (p_func->arguments[arg]->get_datatype() == TYPE_INT && !values.is_empty()) {
									if (values[0].sint < min || values[0].sint > max) {
										error = true;
									}
								} else {
									error = true;
								}

								if (error) {
									_set_error(vformat(RTR("Expected integer constant within [%d..%d] range."), min, max));
									return false;
								}
							}
							constarg_idx++;
						}
					}

					//make sure its not an out argument used in the wrong way
					int outarg_idx = 0;
					while (builtin_func_out_args[outarg_idx].name) {
						if (String(name) == builtin_func_out_args[outarg_idx].name) {
							for (int arg = 0; arg < BuiltinFuncOutArgs::MAX_ARGS; arg++) {
								int arg_idx = builtin_func_out_args[outarg_idx].arguments[arg];
								if (arg_idx == -1) {
									break;
								}
								if (arg_idx < argcount) {
									if (p_func->arguments[arg_idx + 1]->type != Node::NODE_TYPE_VARIABLE && p_func->arguments[arg_idx + 1]->type != Node::NODE_TYPE_MEMBER && p_func->arguments[arg_idx + 1]->type != Node::NODE_TYPE_ARRAY) {
										_set_error(vformat(RTR("Argument %d of function '%s' is not a variable, array, or member."), arg_idx + 1, String(name)));
										return false;
									}

									if (p_func->arguments[arg_idx + 1]->type == Node::NODE_TYPE_ARRAY) {
										ArrayNode *mn = static_cast<ArrayNode *>(p_func->arguments[arg_idx + 1]);
										if (mn->is_const) {
											fail = true;
										}
									} else if (p_func->arguments[arg_idx + 1]->type == Node::NODE_TYPE_MEMBER) {
										MemberNode *mn = static_cast<MemberNode *>(p_func->arguments[arg_idx + 1]);
										if (mn->basetype_const) {
											fail = true;
										}
									} else { // TYPE_VARIABLE
										VariableNode *vn = static_cast<VariableNode *>(p_func->arguments[arg_idx + 1]);
										if (vn->is_const) {
											fail = true;
										} else {
											StringName varname = vn->name;
											if (shader->uniforms.has(varname)) {
												fail = true;
											} else {
												if (shader->varyings.has(varname)) {
													_set_error(vformat(RTR("Varyings cannot be passed for the '%s' parameter."), "out"));
													return false;
												}
												if (p_function_info.built_ins.has(varname)) {
													BuiltInInfo info = p_function_info.built_ins[varname];
													if (info.constant) {
														fail = true;
													}
												}
											}
										}
									}
									if (fail) {
										_set_error(vformat(RTR("A constant value cannot be passed for the '%s' parameter."), "out"));
										return false;
									}

									StringName var_name;
									if (p_func->arguments[arg_idx + 1]->type == Node::NODE_TYPE_ARRAY) {
										var_name = static_cast<const ArrayNode *>(p_func->arguments[arg_idx + 1])->name;
									} else if (p_func->arguments[arg_idx + 1]->type == Node::NODE_TYPE_MEMBER) {
										Node *n = static_cast<const MemberNode *>(p_func->arguments[arg_idx + 1])->owner;
										while (n->type == Node::NODE_TYPE_MEMBER) {
											n = static_cast<const MemberNode *>(n)->owner;
										}
										if (n->type != Node::NODE_TYPE_VARIABLE && n->type != Node::NODE_TYPE_ARRAY) {
											_set_error(vformat(RTR("Argument %d of function '%s' is not a variable, array, or member."), arg_idx + 1, String(name)));
											return false;
										}
										if (n->type == Node::NODE_TYPE_VARIABLE) {
											var_name = static_cast<const VariableNode *>(n)->name;
										} else { // TYPE_ARRAY
											var_name = static_cast<const ArrayNode *>(n)->name;
										}
									} else { // TYPE_VARIABLE
										var_name = static_cast<const VariableNode *>(p_func->arguments[arg_idx + 1])->name;
									}
									const BlockNode *b = p_block;
									bool valid = false;
									while (b) {
										if (b->variables.has(var_name) || p_function_info.built_ins.has(var_name)) {
											valid = true;
											break;
										}
										if (b->parent_function) {
											for (int i = 0; i < b->parent_function->arguments.size(); i++) {
												if (b->parent_function->arguments[i].name == var_name) {
													valid = true;
													break;
												}
											}
										}
										b = b->parent_block;
									}

									if (!valid) {
										_set_error(vformat(RTR("Argument %d of function '%s' can only take a local variable, array, or member."), arg_idx + 1, String(name)));
										return false;
									}
								}
							}
						}
						outarg_idx++;
					}
					//implicitly convert values if possible
					for (int i = 0; i < argcount; i++) {
						if (get_scalar_type(args[i]) != args[i] || args[i] == builtin_func_defs[idx].args[i] || p_func->arguments[i + 1]->type != Node::NODE_TYPE_CONSTANT) {
							//can't do implicit conversion here
							continue;
						}

						//this is an implicit conversion
						ConstantNode *constant = static_cast<ConstantNode *>(p_func->arguments[i + 1]);
						ConstantNode *conversion = alloc_node<ConstantNode>();

						conversion->datatype = builtin_func_defs[idx].args[i];
						conversion->values.resize(1);

						convert_constant(constant, builtin_func_defs[idx].args[i], conversion->values.ptrw());
						p_func->arguments.write[i + 1] = conversion;
					}

					if (r_ret_type) {
						*r_ret_type = builtin_func_defs[idx].rettype;
					}

					return true;
				}
			}

			idx++;
		}
	}

	if (unsupported_builtin) {
		String arglist = "";
		for (int i = 0; i < argcount; i++) {
			if (i > 0) {
				arglist += ", ";
			}
			arglist += get_datatype_name(builtin_func_defs[builtin_idx].args[i]);
		}

		_set_error(vformat(RTR("Built-in function \"%s(%s)\" is only supported on high-end platforms."), String(name), arglist));
		return false;
	}

	if (failed_builtin) {
		String arg_list;
		for (int i = 0; i < argcount; i++) {
			if (i > 0) {
				arg_list += ",";
			}

			String arg_name;
			if (args[i] == TYPE_STRUCT) {
				arg_name = args2[i];
			} else {
				arg_name = get_datatype_name(args[i]);
			}
			if (args3[i] > 0) {
				arg_name += "[";
				arg_name += itos(args3[i]);
				arg_name += "]";
			}
			arg_list += arg_name;
		}
		_set_error(vformat(RTR("Invalid arguments for the built-in function: \"%s(%s)\"."), String(name), arg_list));
		return false;
	}

	// try existing functions..

	StringName exclude_function;
	BlockNode *block = p_block;

	while (block) {
		if (block->parent_function) {
			exclude_function = block->parent_function->name;
		}
		block = block->parent_block;
	}

	if (name == exclude_function) {
		_set_error(RTR("Recursion is not allowed."));
		return false;
	}

	int last_arg_count = 0;
	bool exists = false;
	String arg_list = "";

	for (int i = 0; i < shader->vfunctions.size(); i++) {
		if (name != shader->vfunctions[i].name) {
			continue;
		}
		exists = true;

		if (!shader->vfunctions[i].callable) {
			_set_error(vformat(RTR("Function '%s' can't be called from source code."), String(name)));
			return false;
		}

		FunctionNode *pfunc = shader->vfunctions[i].function;
		if (arg_list.is_empty()) {
			for (int j = 0; j < pfunc->arguments.size(); j++) {
				if (j > 0) {
					arg_list += ", ";
				}
				String func_arg_name;
				if (pfunc->arguments[j].type == TYPE_STRUCT) {
					func_arg_name = pfunc->arguments[j].struct_name;
				} else {
					func_arg_name = get_datatype_name(pfunc->arguments[j].type);
				}
				if (pfunc->arguments[j].array_size > 0) {
					func_arg_name += "[";
					func_arg_name += itos(pfunc->arguments[j].array_size);
					func_arg_name += "]";
				}
				arg_list += func_arg_name;
			}
		}

		if (pfunc->arguments.size() != args.size()) {
			last_arg_count = pfunc->arguments.size();
			continue;
		}

		bool fail = false;
		bool use_constant_conversion = function_overload_count[rname] == 0;

		for (int j = 0; j < args.size(); j++) {
			if (use_constant_conversion && get_scalar_type(args[j]) == args[j] && p_func->arguments[j + 1]->type == Node::NODE_TYPE_CONSTANT && args3[j] == 0 && convert_constant(static_cast<ConstantNode *>(p_func->arguments[j + 1]), pfunc->arguments[j].type)) {
				//all good, but it needs implicit conversion later
			} else if (args[j] != pfunc->arguments[j].type || (args[j] == TYPE_STRUCT && args2[j] != pfunc->arguments[j].struct_name) || args3[j] != pfunc->arguments[j].array_size) {
				String func_arg_name;
				if (pfunc->arguments[j].type == TYPE_STRUCT) {
					func_arg_name = pfunc->arguments[j].struct_name;
				} else {
					func_arg_name = get_datatype_name(pfunc->arguments[j].type);
				}
				if (pfunc->arguments[j].array_size > 0) {
					func_arg_name += "[";
					func_arg_name += itos(pfunc->arguments[j].array_size);
					func_arg_name += "]";
				}
				String arg_name;
				if (args[j] == TYPE_STRUCT) {
					arg_name = args2[j];
				} else {
					arg_name = get_datatype_name(args[j]);
				}
				if (args3[j] > 0) {
					arg_name += "[";
					arg_name += itos(args3[j]);
					arg_name += "]";
				}

				_set_error(vformat(RTR("Invalid argument for \"%s(%s)\" function: argument %d should be %s but is %s."), String(rname), arg_list, j + 1, func_arg_name, arg_name));
				fail = true;
				break;
			}
		}

		if (!fail) {
			//implicitly convert values if possible
			for (int k = 0; k < args.size(); k++) {
				if (get_scalar_type(args[k]) != args[k] || args[k] == pfunc->arguments[k].type || p_func->arguments[k + 1]->type != Node::NODE_TYPE_CONSTANT) {
					//can't do implicit conversion here
					continue;
				}

				//this is an implicit conversion
				ConstantNode *constant = static_cast<ConstantNode *>(p_func->arguments[k + 1]);
				ConstantNode *conversion = alloc_node<ConstantNode>();

				conversion->datatype = pfunc->arguments[k].type;
				conversion->values.resize(1);

				convert_constant(constant, pfunc->arguments[k].type, conversion->values.ptrw());
				p_func->arguments.write[k + 1] = conversion;
			}

			if (r_ret_type) {
				*r_ret_type = pfunc->return_type;
				if (pfunc->return_type == TYPE_STRUCT) {
					*r_ret_type_str = pfunc->return_struct_name;
				}
			}

			if (r_is_custom_function) {
				*r_is_custom_function = true;
			}
			return true;
		}
	}

	if (exists) {
		if (last_arg_count > args.size()) {
			_set_error(vformat(RTR("Too few arguments for \"%s(%s)\" call. Expected at least %d but received %d."), String(rname), arg_list, last_arg_count, args.size()));
		} else if (last_arg_count < args.size()) {
			_set_error(vformat(RTR("Too many arguments for \"%s(%s)\" call. Expected at most %d but received %d."), String(rname), arg_list, last_arg_count, args.size()));
		}
	}

	return false;
}

bool ShaderLanguage::_compare_datatypes(DataType p_datatype_a, String p_datatype_name_a, int p_array_size_a, DataType p_datatype_b, String p_datatype_name_b, int p_array_size_b) {
	bool result = true;

	if (p_datatype_a == TYPE_STRUCT || p_datatype_b == TYPE_STRUCT) {
		if (p_datatype_name_a != p_datatype_name_b) {
			result = false;
		}
	} else {
		if (p_datatype_a != p_datatype_b) {
			result = false;
		}
	}

	if (p_array_size_a != p_array_size_b) {
		result = false;
	}

	if (!result) {
		String type_name = p_datatype_a == TYPE_STRUCT ? p_datatype_name_a : get_datatype_name(p_datatype_a);
		if (p_array_size_a > 0) {
			type_name += "[";
			type_name += itos(p_array_size_a);
			type_name += "]";
		}

		String type_name2 = p_datatype_b == TYPE_STRUCT ? p_datatype_name_b : get_datatype_name(p_datatype_b);
		if (p_array_size_b > 0) {
			type_name2 += "[";
			type_name2 += itos(p_array_size_b);
			type_name2 += "]";
		}

		_set_error(vformat(RTR("Invalid assignment of '%s' to '%s'."), type_name2, type_name));
	}
	return result;
}

bool ShaderLanguage::_compare_datatypes_in_nodes(Node *a, Node *b) {
	return _compare_datatypes(a->get_datatype(), a->get_datatype_name(), a->get_array_size(), b->get_datatype(), b->get_datatype_name(), b->get_array_size());
}

bool ShaderLanguage::_parse_function_arguments(BlockNode *p_block, const FunctionInfo &p_function_info, OperatorNode *p_func, int *r_complete_arg) {
	TkPos pos = _get_tkpos();
	Token tk = _get_token();

	if (tk.type == TK_PARENTHESIS_CLOSE) {
		return true;
	}

	_set_tkpos(pos);

	while (true) {
		if (r_complete_arg) {
			pos = _get_tkpos();
			tk = _get_token();

			if (tk.type == TK_CURSOR) {
				*r_complete_arg = p_func->arguments.size() - 1;
			} else {
				_set_tkpos(pos);
			}
		}

		Node *arg = _parse_and_reduce_expression(p_block, p_function_info);

		if (!arg) {
			return false;
		}

		if (is_const_decl && arg->type == Node::NODE_TYPE_VARIABLE) {
			const VariableNode *var = static_cast<const VariableNode *>(arg);
			if (!var->is_const) {
				_set_error(RTR("Expected constant expression."));
				return false;
			}
		}

		p_func->arguments.push_back(arg);

		tk = _get_token();

		if (tk.type == TK_PARENTHESIS_CLOSE) {
			return true;
		} else if (tk.type != TK_COMMA) {
			// something is broken
			_set_error(RTR("Expected ',' or ')' after argument."));
			return false;
		}
	}

	return true;
}

bool ShaderLanguage::is_token_operator(TokenType p_type) {
	return (p_type == TK_OP_EQUAL ||
			p_type == TK_OP_NOT_EQUAL ||
			p_type == TK_OP_LESS ||
			p_type == TK_OP_LESS_EQUAL ||
			p_type == TK_OP_GREATER ||
			p_type == TK_OP_GREATER_EQUAL ||
			p_type == TK_OP_AND ||
			p_type == TK_OP_OR ||
			p_type == TK_OP_NOT ||
			p_type == TK_OP_ADD ||
			p_type == TK_OP_SUB ||
			p_type == TK_OP_MUL ||
			p_type == TK_OP_DIV ||
			p_type == TK_OP_MOD ||
			p_type == TK_OP_SHIFT_LEFT ||
			p_type == TK_OP_SHIFT_RIGHT ||
			p_type == TK_OP_ASSIGN ||
			p_type == TK_OP_ASSIGN_ADD ||
			p_type == TK_OP_ASSIGN_SUB ||
			p_type == TK_OP_ASSIGN_MUL ||
			p_type == TK_OP_ASSIGN_DIV ||
			p_type == TK_OP_ASSIGN_MOD ||
			p_type == TK_OP_ASSIGN_SHIFT_LEFT ||
			p_type == TK_OP_ASSIGN_SHIFT_RIGHT ||
			p_type == TK_OP_ASSIGN_BIT_AND ||
			p_type == TK_OP_ASSIGN_BIT_OR ||
			p_type == TK_OP_ASSIGN_BIT_XOR ||
			p_type == TK_OP_BIT_AND ||
			p_type == TK_OP_BIT_OR ||
			p_type == TK_OP_BIT_XOR ||
			p_type == TK_OP_BIT_INVERT ||
			p_type == TK_OP_INCREMENT ||
			p_type == TK_OP_DECREMENT ||
			p_type == TK_QUESTION ||
			p_type == TK_COLON);
}

bool ShaderLanguage::is_token_operator_assign(TokenType p_type) {
	return (p_type == TK_OP_ASSIGN ||
			p_type == TK_OP_ASSIGN_ADD ||
			p_type == TK_OP_ASSIGN_SUB ||
			p_type == TK_OP_ASSIGN_MUL ||
			p_type == TK_OP_ASSIGN_DIV ||
			p_type == TK_OP_ASSIGN_MOD ||
			p_type == TK_OP_ASSIGN_SHIFT_LEFT ||
			p_type == TK_OP_ASSIGN_SHIFT_RIGHT ||
			p_type == TK_OP_ASSIGN_BIT_AND ||
			p_type == TK_OP_ASSIGN_BIT_OR ||
			p_type == TK_OP_ASSIGN_BIT_XOR);
}

bool ShaderLanguage::is_token_hint(TokenType p_type) {
	return int(p_type) > int(TK_RENDER_MODE) && int(p_type) < int(TK_SHADER_TYPE);
}

bool ShaderLanguage::convert_constant(ConstantNode *p_constant, DataType p_to_type, Scalar *p_value) {
	if (p_constant->datatype == p_to_type) {
		if (p_value) {
			for (int i = 0; i < p_constant->values.size(); i++) {
				p_value[i] = p_constant->values[i];
			}
		}
		return true;
	} else if (p_constant->datatype == TYPE_INT && p_to_type == TYPE_FLOAT) {
		if (p_value) {
			p_value->real = p_constant->values[0].sint;
		}
		return true;
	} else if (p_constant->datatype == TYPE_UINT && p_to_type == TYPE_FLOAT) {
		if (p_value) {
			p_value->real = p_constant->values[0].uint;
		}
		return true;
	} else if (p_constant->datatype == TYPE_INT && p_to_type == TYPE_UINT) {
		if (p_constant->values[0].sint < 0) {
			return false;
		}
		if (p_value) {
			p_value->uint = p_constant->values[0].sint;
		}
		return true;
	} else if (p_constant->datatype == TYPE_UINT && p_to_type == TYPE_INT) {
		if (p_constant->values[0].uint > 0x7FFFFFFF) {
			return false;
		}
		if (p_value) {
			p_value->sint = p_constant->values[0].uint;
		}
		return true;
	} else {
		return false;
	}
}

bool ShaderLanguage::is_scalar_type(DataType p_type) {
	return p_type == TYPE_BOOL || p_type == TYPE_INT || p_type == TYPE_UINT || p_type == TYPE_FLOAT;
}

bool ShaderLanguage::is_float_type(DataType p_type) {
	switch (p_type) {
		case TYPE_FLOAT:
		case TYPE_VEC2:
		case TYPE_VEC3:
		case TYPE_VEC4:
		case TYPE_MAT2:
		case TYPE_MAT3:
		case TYPE_MAT4:
		case TYPE_SAMPLER2D:
		case TYPE_SAMPLER2DARRAY:
		case TYPE_SAMPLER3D:
		case TYPE_SAMPLERCUBE:
		case TYPE_SAMPLERCUBEARRAY: {
			return true;
		}
		default: {
			return false;
		}
	}
}
bool ShaderLanguage::is_sampler_type(DataType p_type) {
	return p_type > TYPE_MAT4 && p_type < TYPE_STRUCT;
}

Variant ShaderLanguage::constant_value_to_variant(const Vector<Scalar> &p_value, DataType p_type, int p_array_size, ShaderLanguage::ShaderNode::Uniform::Hint p_hint) {
	int array_size = p_array_size;

	if (p_value.size() > 0) {
		Variant value;
		switch (p_type) {
			case ShaderLanguage::TYPE_BOOL:
				if (array_size > 0) {
					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].boolean);
					}
					value = Variant(array);
				} else {
					value = Variant(p_value[0].boolean);
				}
				break;
			case ShaderLanguage::TYPE_BVEC2:
				array_size *= 2;

				if (array_size > 0) {
					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].boolean);
					}
					value = Variant(array);
				} else {
					value = Variant(p_value[0].boolean);
				}
				break;
			case ShaderLanguage::TYPE_BVEC3:
				array_size *= 3;

				if (array_size > 0) {
					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].boolean);
					}
					value = Variant(array);
				} else {
					value = Variant(p_value[0].boolean);
				}
				break;
			case ShaderLanguage::TYPE_BVEC4:
				array_size *= 4;

				if (array_size > 0) {
					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].boolean);
					}
					value = Variant(array);
				} else {
					value = Variant(p_value[0].boolean);
				}
				break;
			case ShaderLanguage::TYPE_INT:
				if (array_size > 0) {
					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].sint);
					}
					value = Variant(array);
				} else {
					value = Variant(p_value[0].sint);
				}
				break;
			case ShaderLanguage::TYPE_IVEC2:
				if (array_size > 0) {
					array_size *= 2;

					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].sint);
					}
					value = Variant(array);
				} else {
					value = Variant(Vector2i(p_value[0].sint, p_value[1].sint));
				}
				break;
			case ShaderLanguage::TYPE_IVEC3:
				if (array_size > 0) {
					array_size *= 3;

					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].sint);
					}
					value = Variant(array);
				} else {
					value = Variant(Vector3i(p_value[0].sint, p_value[1].sint, p_value[2].sint));
				}
				break;
			case ShaderLanguage::TYPE_IVEC4:
				if (array_size > 0) {
					array_size *= 4;

					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].sint);
					}
					value = Variant(array);
				} else {
					value = Variant(Vector4i(p_value[0].sint, p_value[1].sint, p_value[2].sint, p_value[3].sint));
				}
				break;
			case ShaderLanguage::TYPE_UINT:
				if (array_size > 0) {
					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].uint);
					}
					value = Variant(array);
				} else {
					value = Variant(p_value[0].uint);
				}
				break;
			case ShaderLanguage::TYPE_UVEC2:
				if (array_size > 0) {
					array_size *= 2;

					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].uint);
					}
					value = Variant(array);
				} else {
					value = Variant(Vector2i(p_value[0].uint, p_value[1].uint));
				}
				break;
			case ShaderLanguage::TYPE_UVEC3:
				if (array_size > 0) {
					array_size *= 3;

					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].uint);
					}
					value = Variant(array);
				} else {
					value = Variant(Vector3i(p_value[0].uint, p_value[1].uint, p_value[2].uint));
				}
				break;
			case ShaderLanguage::TYPE_UVEC4:
				if (array_size > 0) {
					array_size *= 4;

					PackedInt32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].uint);
					}
					value = Variant(array);
				} else {
					value = Variant(Vector4i(p_value[0].uint, p_value[1].uint, p_value[2].uint, p_value[3].uint));
				}
				break;
			case ShaderLanguage::TYPE_FLOAT:
				if (array_size > 0) {
					PackedFloat32Array array;
					for (int i = 0; i < array_size; i++) {
						array.push_back(p_value[i].real);
					}
					value = Variant(array);
				} else {
					value = Variant(p_value[0].real);
				}
				break;
			case ShaderLanguage::TYPE_VEC2:
				if (array_size > 0) {
					array_size *= 2;

					PackedVector2Array array;
					for (int i = 0; i < array_size; i += 2) {
						array.push_back(Vector2(p_value[i].real, p_value[i + 1].real));
					}
					value = Variant(array);
				} else {
					value = Variant(Vector2(p_value[0].real, p_value[1].real));
				}
				break;
			case ShaderLanguage::TYPE_VEC3:
				if (array_size > 0) {
					array_size *= 3;

					if (p_hint == ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR) {
						PackedColorArray array;
						for (int i = 0; i < array_size; i += 3) {
							array.push_back(Color(p_value[i].real, p_value[i + 1].real, p_value[i + 2].real));
						}
						value = Variant(array);
					} else {
						PackedVector3Array array;
						for (int i = 0; i < array_size; i += 3) {
							array.push_back(Vector3(p_value[i].real, p_value[i + 1].real, p_value[i + 2].real));
						}
						value = Variant(array);
					}
				} else {
					if (p_hint == ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR) {
						value = Variant(Color(p_value[0].real, p_value[1].real, p_value[2].real));
					} else {
						value = Variant(Vector3(p_value[0].real, p_value[1].real, p_value[2].real));
					}
				}
				break;
			case ShaderLanguage::TYPE_VEC4:
				if (array_size > 0) {
					array_size *= 4;

					if (p_hint == ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR) {
						PackedColorArray array;
						for (int i = 0; i < array_size; i += 4) {
							array.push_back(Color(p_value[i].real, p_value[i + 1].real, p_value[i + 2].real, p_value[i + 3].real));
						}
						value = Variant(array);
					} else {
						PackedVector4Array array;
						for (int i = 0; i < array_size; i += 4) {
							array.push_back(Vector4(p_value[i].real, p_value[i + 1].real, p_value[i + 2].real, p_value[i + 3].real));
						}
						value = Variant(array);
					}
				} else {
					if (p_hint == ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR) {
						value = Variant(Color(p_value[0].real, p_value[1].real, p_value[2].real, p_value[3].real));
					} else {
						value = Variant(Vector4(p_value[0].real, p_value[1].real, p_value[2].real, p_value[3].real));
					}
				}
				break;
			case ShaderLanguage::TYPE_MAT2:
				if (array_size > 0) {
					array_size *= 4;

					PackedFloat32Array array;
					for (int i = 0; i < array_size; i += 4) {
						array.push_back(p_value[i].real);
						array.push_back(p_value[i + 1].real);
						array.push_back(p_value[i + 2].real);
						array.push_back(p_value[i + 3].real);
					}
					value = Variant(array);
				} else {
					value = Variant(Transform2D(p_value[0].real, p_value[2].real, p_value[1].real, p_value[3].real, 0.0, 0.0));
				}
				break;
			case ShaderLanguage::TYPE_MAT3: {
				if (array_size > 0) {
					array_size *= 9;

					PackedFloat32Array array;
					for (int i = 0; i < array_size; i += 9) {
						for (int j = 0; j < 9; j++) {
							array.push_back(p_value[i + j].real);
						}
					}
					value = Variant(array);
				} else {
					Basis p;
					p[0][0] = p_value[0].real;
					p[0][1] = p_value[1].real;
					p[0][2] = p_value[2].real;
					p[1][0] = p_value[3].real;
					p[1][1] = p_value[4].real;
					p[1][2] = p_value[5].real;
					p[2][0] = p_value[6].real;
					p[2][1] = p_value[7].real;
					p[2][2] = p_value[8].real;
					value = Variant(p);
				}
				break;
			}
			case ShaderLanguage::TYPE_MAT4: {
				if (array_size > 0) {
					array_size *= 16;

					PackedFloat32Array array;
					for (int i = 0; i < array_size; i += 16) {
						for (int j = 0; j < 16; j++) {
							array.push_back(p_value[i + j].real);
						}
					}
					value = Variant(array);
				} else {
					Projection p = Projection(Vector4(p_value[0].real, p_value[1].real, p_value[2].real, p_value[3].real),
							Vector4(p_value[4].real, p_value[5].real, p_value[6].real, p_value[7].real),
							Vector4(p_value[8].real, p_value[9].real, p_value[10].real, p_value[11].real),
							Vector4(p_value[12].real, p_value[13].real, p_value[14].real, p_value[15].real));
					value = Variant(p);
				}
				break;
			}
			case ShaderLanguage::TYPE_ISAMPLER2DARRAY:
			case ShaderLanguage::TYPE_ISAMPLER2D:
			case ShaderLanguage::TYPE_ISAMPLER3D:
			case ShaderLanguage::TYPE_SAMPLER2DARRAY:
			case ShaderLanguage::TYPE_SAMPLER2D:
			case ShaderLanguage::TYPE_SAMPLER3D:
			case ShaderLanguage::TYPE_USAMPLER2DARRAY:
			case ShaderLanguage::TYPE_USAMPLER2D:
			case ShaderLanguage::TYPE_USAMPLER3D:
			case ShaderLanguage::TYPE_SAMPLERCUBE:
			case ShaderLanguage::TYPE_SAMPLERCUBEARRAY:
			case ShaderLanguage::TYPE_SAMPLEREXT: {
				// Texture types, likely not relevant here.
				break;
			}
			case ShaderLanguage::TYPE_STRUCT:
				break;
			case ShaderLanguage::TYPE_VOID:
				break;
			case ShaderLanguage::TYPE_MAX:
				break;
		}
		return value;
	}
	return Variant();
}

PropertyInfo ShaderLanguage::uniform_to_property_info(const ShaderNode::Uniform &p_uniform) {
	PropertyInfo pi;
	switch (p_uniform.type) {
		case ShaderLanguage::TYPE_VOID:
			pi.type = Variant::NIL;
			break;
		case ShaderLanguage::TYPE_BOOL:
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_INT32_ARRAY;
				pi.hint = PROPERTY_HINT_TYPE_STRING;
				pi.hint_string = itos(Variant::INT) + "/" + itos(PROPERTY_HINT_FLAGS) + ":" + RTR("On");
			} else {
				pi.type = Variant::BOOL;
			}
			break;
		case ShaderLanguage::TYPE_BVEC2:
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_INT32_ARRAY;
				pi.hint = PROPERTY_HINT_TYPE_STRING;
				pi.hint_string = itos(Variant::INT) + "/" + itos(PROPERTY_HINT_FLAGS) + ":x,y";
			} else {
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_FLAGS;
				pi.hint_string = "x,y";
			}
			break;
		case ShaderLanguage::TYPE_BVEC3:
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_INT32_ARRAY;
				pi.hint = PROPERTY_HINT_TYPE_STRING;
				pi.hint_string = itos(Variant::INT) + "/" + itos(PROPERTY_HINT_FLAGS) + ":x,y,z";
			} else {
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_FLAGS;
				pi.hint_string = "x,y,z";
			}
			break;
		case ShaderLanguage::TYPE_BVEC4:
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_INT32_ARRAY;
				pi.hint = PROPERTY_HINT_TYPE_STRING;
				pi.hint_string = itos(Variant::INT) + "/" + itos(PROPERTY_HINT_FLAGS) + ":x,y,z,w";
			} else {
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_FLAGS;
				pi.hint_string = "x,y,z,w";
			}
			break;
		case ShaderLanguage::TYPE_UINT:
		case ShaderLanguage::TYPE_INT: {
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_INT32_ARRAY;
				// TODO: Handle range and encoding for for unsigned values.
			} else if (p_uniform.hint == ShaderLanguage::ShaderNode::Uniform::HINT_ENUM) {
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_ENUM;
				String hint_string;
				pi.hint_string = String(",").join(p_uniform.hint_enum_names);
			} else {
				pi.type = Variant::INT;
				pi.hint = PROPERTY_HINT_RANGE;
				if (p_uniform.hint == ShaderLanguage::ShaderNode::Uniform::HINT_RANGE) {
					pi.hint_string = rtos(p_uniform.hint_range[0]) + "," + rtos(p_uniform.hint_range[1]) + "," + rtos(p_uniform.hint_range[2]);
				} else if (p_uniform.type == ShaderLanguage::TYPE_UINT) {
					pi.hint_string = "0," + itos(UINT32_MAX);
				} else {
					pi.hint_string = itos(INT32_MIN) + "," + itos(INT32_MAX);
				}
			}
		} break;
		case ShaderLanguage::TYPE_UVEC2:
		case ShaderLanguage::TYPE_IVEC2: {
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_INT32_ARRAY;
				// TODO: Handle vector pairs?
			} else {
				pi.type = Variant::VECTOR2I;
			}
		} break;
		case ShaderLanguage::TYPE_UVEC3:
		case ShaderLanguage::TYPE_IVEC3: {
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_INT32_ARRAY;
				// TODO: Handle vector pairs?
			} else {
				pi.type = Variant::VECTOR3I;
			}
		} break;
		case ShaderLanguage::TYPE_UVEC4:
		case ShaderLanguage::TYPE_IVEC4: {
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_INT32_ARRAY;
				// TODO: Handle vector pairs?
			} else {
				pi.type = Variant::VECTOR4I;
			}
		} break;
		case ShaderLanguage::TYPE_FLOAT: {
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_FLOAT32_ARRAY;
			} else {
				pi.type = Variant::FLOAT;
				if (p_uniform.hint == ShaderLanguage::ShaderNode::Uniform::HINT_RANGE) {
					pi.hint = PROPERTY_HINT_RANGE;
					pi.hint_string = rtos(p_uniform.hint_range[0]) + "," + rtos(p_uniform.hint_range[1]) + "," + rtos(p_uniform.hint_range[2]);
				}
			}
		} break;
		case ShaderLanguage::TYPE_VEC2:
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_VECTOR2_ARRAY;
			} else {
				pi.type = Variant::VECTOR2;
			}
			break;
		case ShaderLanguage::TYPE_VEC3:
			if (p_uniform.array_size > 0) {
				if (p_uniform.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR) {
					pi.hint = PROPERTY_HINT_COLOR_NO_ALPHA;
					pi.type = Variant::PACKED_COLOR_ARRAY;
				} else {
					pi.type = Variant::PACKED_VECTOR3_ARRAY;
				}
			} else {
				if (p_uniform.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR) {
					pi.hint = PROPERTY_HINT_COLOR_NO_ALPHA;
					pi.type = Variant::COLOR;
				} else {
					pi.type = Variant::VECTOR3;
				}
			}
			break;
		case ShaderLanguage::TYPE_VEC4: {
			if (p_uniform.array_size > 0) {
				if (p_uniform.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR) {
					pi.type = Variant::PACKED_COLOR_ARRAY;
				} else {
					pi.type = Variant::PACKED_VECTOR4_ARRAY;
				}
			} else {
				if (p_uniform.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR) {
					pi.type = Variant::COLOR;
				} else {
					pi.type = Variant::VECTOR4;
				}
			}
		} break;
		case ShaderLanguage::TYPE_MAT2:
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_FLOAT32_ARRAY;
			} else {
				pi.type = Variant::TRANSFORM2D;
			}
			break;
		case ShaderLanguage::TYPE_MAT3:
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_FLOAT32_ARRAY;
			} else {
				pi.type = Variant::BASIS;
			}
			break;
		case ShaderLanguage::TYPE_MAT4:
			if (p_uniform.array_size > 0) {
				pi.type = Variant::PACKED_FLOAT32_ARRAY;
			} else {
				pi.type = Variant::PROJECTION;
			}
			break;
		case ShaderLanguage::TYPE_SAMPLER2D:
		case ShaderLanguage::TYPE_ISAMPLER2D:
		case ShaderLanguage::TYPE_USAMPLER2D: {
			if (p_uniform.array_size > 0) {
				pi.type = Variant::ARRAY;
				pi.hint = PROPERTY_HINT_ARRAY_TYPE;
				pi.hint_string = MAKE_RESOURCE_TYPE_HINT("Texture2D");
			} else {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "Texture2D";
			}
		} break;
		case ShaderLanguage::TYPE_SAMPLER2DARRAY:
		case ShaderLanguage::TYPE_ISAMPLER2DARRAY:
		case ShaderLanguage::TYPE_USAMPLER2DARRAY:
		case ShaderLanguage::TYPE_SAMPLERCUBE:
		case ShaderLanguage::TYPE_SAMPLERCUBEARRAY: {
			if (p_uniform.array_size > 0) {
				pi.type = Variant::ARRAY;
				pi.hint = PROPERTY_HINT_ARRAY_TYPE;
				pi.hint_string = MAKE_RESOURCE_TYPE_HINT("TextureLayered");
			} else {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "TextureLayered";
			}
		} break;
		case ShaderLanguage::TYPE_SAMPLER3D:
		case ShaderLanguage::TYPE_ISAMPLER3D:
		case ShaderLanguage::TYPE_USAMPLER3D: {
			if (p_uniform.array_size > 0) {
				pi.type = Variant::ARRAY;
				pi.hint = PROPERTY_HINT_ARRAY_TYPE;
				pi.hint_string = MAKE_RESOURCE_TYPE_HINT("Texture3D");
			} else {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "Texture3D";
			}
		} break;
		case ShaderLanguage::TYPE_SAMPLEREXT: {
			if (p_uniform.array_size > 0) {
				pi.type = Variant::ARRAY;
				pi.hint = PROPERTY_HINT_ARRAY_TYPE;
				pi.hint_string = MAKE_RESOURCE_TYPE_HINT("ExternalTexture");
			} else {
				pi.type = Variant::OBJECT;
				pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
				pi.hint_string = "ExternalTexture";
			}
		} break;
		case ShaderLanguage::TYPE_STRUCT: {
			// FIXME: Implement this.
		} break;
		case ShaderLanguage::TYPE_MAX:
			break;
	}
	return pi;
}

uint32_t ShaderLanguage::get_datatype_size(ShaderLanguage::DataType p_type) {
	switch (p_type) {
		case TYPE_VOID:
			return 0;
		case TYPE_BOOL:
			return 4;
		case TYPE_BVEC2:
			return 8;
		case TYPE_BVEC3:
			return 12;
		case TYPE_BVEC4:
			return 16;
		case TYPE_INT:
			return 4;
		case TYPE_IVEC2:
			return 8;
		case TYPE_IVEC3:
			return 12;
		case TYPE_IVEC4:
			return 16;
		case TYPE_UINT:
			return 4;
		case TYPE_UVEC2:
			return 8;
		case TYPE_UVEC3:
			return 12;
		case TYPE_UVEC4:
			return 16;
		case TYPE_FLOAT:
			return 4;
		case TYPE_VEC2:
			return 8;
		case TYPE_VEC3:
			return 12;
		case TYPE_VEC4:
			return 16;
		case TYPE_MAT2:
			return 32; // 4 * 4 + 4 * 4
		case TYPE_MAT3:
			return 48; // 4 * 4 + 4 * 4 + 4 * 4
		case TYPE_MAT4:
			return 64;
		case TYPE_SAMPLER2D:
			return 16;
		case TYPE_ISAMPLER2D:
			return 16;
		case TYPE_USAMPLER2D:
			return 16;
		case TYPE_SAMPLER2DARRAY:
			return 16;
		case TYPE_ISAMPLER2DARRAY:
			return 16;
		case TYPE_USAMPLER2DARRAY:
			return 16;
		case TYPE_SAMPLER3D:
			return 16;
		case TYPE_ISAMPLER3D:
			return 16;
		case TYPE_USAMPLER3D:
			return 16;
		case TYPE_SAMPLERCUBE:
			return 16;
		case TYPE_SAMPLERCUBEARRAY:
			return 16;
		case TYPE_SAMPLEREXT:
			return 16;
		case TYPE_STRUCT:
			return 0;
		case TYPE_MAX: {
			ERR_FAIL_V(0);
		};
	}
	ERR_FAIL_V(0);
}

uint32_t ShaderLanguage::get_datatype_component_count(ShaderLanguage::DataType p_type) {
	switch (p_type) {
		case TYPE_BOOL:
			return 1U;
		case TYPE_BVEC2:
			return 2U;
		case TYPE_BVEC3:
			return 3U;
		case TYPE_BVEC4:
			return 4U;
		case TYPE_INT:
			return 1U;
		case TYPE_IVEC2:
			return 2U;
		case TYPE_IVEC3:
			return 3U;
		case TYPE_IVEC4:
			return 4U;
		case TYPE_UINT:
			return 1U;
		case TYPE_UVEC2:
			return 2U;
		case TYPE_UVEC3:
			return 3U;
		case TYPE_UVEC4:
			return 4U;
		case TYPE_FLOAT:
			return 1U;
		case TYPE_VEC2:
			return 2U;
		case TYPE_VEC3:
			return 3U;
		case TYPE_VEC4:
			return 4U;
		case TYPE_MAT2:
			return 4U;
		case TYPE_MAT3:
			return 9U;
		case TYPE_MAT4:
			return 16U;
		default:
			break;
	}
	return 0U;
}

void ShaderLanguage::get_keyword_list(List<String> *r_keywords) {
	HashSet<String> kws;

	int idx = 0;

	while (keyword_list[idx].text) {
		kws.insert(keyword_list[idx].text);
		idx++;
	}

	idx = 0;

	while (builtin_func_defs[idx].name) {
		kws.insert(builtin_func_defs[idx].name);

		idx++;
	}

	for (const String &E : kws) {
		r_keywords->push_back(E);
	}
}

bool ShaderLanguage::is_control_flow_keyword(String p_keyword) {
	return p_keyword == "break" ||
			p_keyword == "case" ||
			p_keyword == "continue" ||
			p_keyword == "default" ||
			p_keyword == "do" ||
			p_keyword == "else" ||
			p_keyword == "for" ||
			p_keyword == "if" ||
			p_keyword == "return" ||
			p_keyword == "switch" ||
			p_keyword == "while";
}

void ShaderLanguage::get_builtin_funcs(List<String> *r_keywords) {
	HashSet<String> kws;

	int idx = 0;

	while (builtin_func_defs[idx].name) {
		kws.insert(builtin_func_defs[idx].name);

		idx++;
	}

	for (const String &E : kws) {
		r_keywords->push_back(E);
	}
}

ShaderLanguage::DataType ShaderLanguage::get_scalar_type(DataType p_type) {
	static const DataType scalar_types[] = {
		TYPE_VOID,
		TYPE_BOOL,
		TYPE_BOOL,
		TYPE_BOOL,
		TYPE_BOOL,
		TYPE_INT,
		TYPE_INT,
		TYPE_INT,
		TYPE_INT,
		TYPE_UINT,
		TYPE_UINT,
		TYPE_UINT,
		TYPE_UINT,
		TYPE_FLOAT,
		TYPE_FLOAT,
		TYPE_FLOAT,
		TYPE_FLOAT,
		TYPE_FLOAT,
		TYPE_FLOAT,
		TYPE_FLOAT,
		TYPE_FLOAT,
		TYPE_INT,
		TYPE_UINT,
		TYPE_FLOAT,
		TYPE_INT,
		TYPE_UINT,
		TYPE_FLOAT,
		TYPE_INT,
		TYPE_UINT,
		TYPE_FLOAT,
		TYPE_FLOAT,
		TYPE_FLOAT,
		TYPE_VOID,
	};

	static_assert(sizeof(scalar_types) / sizeof(*scalar_types) == TYPE_MAX);

	return scalar_types[p_type];
}

int ShaderLanguage::get_cardinality(DataType p_type) {
	static const int cardinality_table[] = {
		0,
		1,
		2,
		3,
		4,
		1,
		2,
		3,
		4,
		1,
		2,
		3,
		4,
		1,
		2,
		3,
		4,
		4,
		9,
		16,
		1,
		1,
		1,
		1,
		1,
		1,
		1,
		1,
		1,
		1,
		1,
		1,
		1,
	};

	static_assert(sizeof(cardinality_table) / sizeof(*cardinality_table) == TYPE_MAX);

	return cardinality_table[p_type];
}

bool ShaderLanguage::_get_completable_identifier(BlockNode *p_block, CompletionType p_type, StringName &identifier) {
	identifier = StringName();

	TkPos pos = { 0, 0 };

	Token tk = _get_token();

	if (tk.type == TK_IDENTIFIER) {
		identifier = tk.text;
		pos = _get_tkpos();
		tk = _get_token();
	}

	if (tk.type == TK_CURSOR) {
		completion_type = p_type;
		completion_line = tk_line;
		completion_block = p_block;

		pos = _get_tkpos();
		tk = _get_token();

		if (tk.type == TK_IDENTIFIER) {
			identifier = identifier.operator String() + tk.text.operator String();
		} else {
			_set_tkpos(pos);
		}
		return true;
	} else if (identifier != StringName()) {
		_set_tkpos(pos);
	}

	return false;
}

bool ShaderLanguage::_is_operator_assign(Operator p_op) const {
	switch (p_op) {
		case OP_ASSIGN:
		case OP_ASSIGN_ADD:
		case OP_ASSIGN_SUB:
		case OP_ASSIGN_MUL:
		case OP_ASSIGN_DIV:
		case OP_ASSIGN_MOD:
		case OP_ASSIGN_SHIFT_LEFT:
		case OP_ASSIGN_SHIFT_RIGHT:
		case OP_ASSIGN_BIT_AND:
		case OP_ASSIGN_BIT_OR:
		case OP_ASSIGN_BIT_XOR:
			return true;
		default:
			return false;
	}
}

bool ShaderLanguage::_validate_varying_assign(ShaderNode::Varying &p_varying, String *r_message) {
	if (current_function != "vertex" && current_function != "fragment") {
		*r_message = vformat(RTR("Varying may not be assigned in the '%s' function."), current_function);
		return false;
	}
	switch (p_varying.stage) {
		case ShaderNode::Varying::STAGE_UNKNOWN: // first assign
			if (current_function == varying_function_names.vertex) {
				if (p_varying.type < TYPE_INT) {
					*r_message = vformat(RTR("Varying with '%s' data type may only be assigned in the 'fragment' function."), get_datatype_name(p_varying.type));
					return false;
				}
				p_varying.stage = ShaderNode::Varying::STAGE_VERTEX;
			} else if (current_function == varying_function_names.fragment) {
				p_varying.stage = ShaderNode::Varying::STAGE_FRAGMENT;
			}
			break;
		case ShaderNode::Varying::STAGE_VERTEX_TO_FRAGMENT_LIGHT:
		case ShaderNode::Varying::STAGE_VERTEX:
			if (current_function == varying_function_names.fragment) {
				*r_message = RTR("Varyings which assigned in 'vertex' function may not be reassigned in 'fragment' or 'light'.");
				return false;
			}
			break;
		case ShaderNode::Varying::STAGE_FRAGMENT_TO_LIGHT:
		case ShaderNode::Varying::STAGE_FRAGMENT:
			if (current_function == varying_function_names.vertex) {
				*r_message = RTR("Varyings which assigned in 'fragment' function may not be reassigned in 'vertex' or 'light'.");
				return false;
			}
			break;
		default:
			break;
	}
	return true;
}

bool ShaderLanguage::_check_node_constness(const Node *p_node) const {
	switch (p_node->type) {
		case Node::NODE_TYPE_OPERATOR: {
			const OperatorNode *op_node = static_cast<const OperatorNode *>(p_node);
			for (int i = int(op_node->op == OP_CALL); i < op_node->arguments.size(); i++) {
				if (!_check_node_constness(op_node->arguments[i])) {
					return false;
				}
			}
		} break;
		case Node::NODE_TYPE_CONSTANT:
			break;
		case Node::NODE_TYPE_VARIABLE: {
			const VariableNode *var_node = static_cast<const VariableNode *>(p_node);
			if (!var_node->is_const) {
				return false;
			}
		} break;
		case Node::NODE_TYPE_ARRAY: {
			const ArrayNode *arr_node = static_cast<const ArrayNode *>(p_node);
			if (!arr_node->is_const) {
				return false;
			}
		} break;
		default:
			return false;
	}
	return true;
}

bool ShaderLanguage::_check_restricted_func(const StringName &p_name, const StringName &p_current_function) const {
	int idx = 0;

	while (frag_only_func_defs[idx].name) {
		if (StringName(frag_only_func_defs[idx].name) == p_name) {
			if (is_supported_frag_only_funcs) {
				if (p_current_function == "vertex" && stages->has(p_current_function)) {
					return true;
				}
			} else {
				return true;
			}
			break;
		}
		idx++;
	}

	return false;
}

bool ShaderLanguage::_validate_restricted_func(const StringName &p_name, const CallInfo *p_func_info, bool p_is_builtin_hint) {
	const bool is_in_restricted_function = p_func_info->name == "vertex";

	// No need to check up the hierarchy if it's a built-in.
	if (!p_is_builtin_hint) {
		for (const CallInfo *func_info : p_func_info->calls) {
			if (is_in_restricted_function && func_info->name != p_name) {
				// Skips check for non-called method.
				continue;
			}

			if (!_validate_restricted_func(p_name, func_info)) {
				return false;
			}
		}
	}

	if (!p_func_info->uses_restricted_items.is_empty()) {
		const Pair<StringName, CallInfo::Item> &first_element = p_func_info->uses_restricted_items.get(0);

		if (first_element.second.type == CallInfo::Item::ITEM_TYPE_VARYING) {
			const ShaderNode::Varying &varying = shader->varyings[first_element.first];

			if (varying.stage == ShaderNode::Varying::STAGE_VERTEX) {
				return true;
			}
		}

		_set_tkpos(first_element.second.pos);

		if (is_in_restricted_function) {
			_set_error(vformat(RTR("'%s' cannot be used within the '%s' processor function."), first_element.first, "vertex"));
		} else {
			_set_error(vformat(RTR("'%s' cannot be used here, because '%s' is called by the '%s' processor function (which is not allowed)."), first_element.first, p_func_info->name, "vertex"));
		}
		return false;
	}

	return true;
}

bool ShaderLanguage::_validate_assign(Node *p_node, const FunctionInfo &p_function_info, String *r_message) {
	if (p_node->type == Node::NODE_TYPE_OPERATOR) {
		OperatorNode *op = static_cast<OperatorNode *>(p_node);

		if (op->op == OP_INDEX) {
			return _validate_assign(op->arguments[0], p_function_info, r_message);

		} else if (_is_operator_assign(op->op)) {
			//chained assignment
			return _validate_assign(op->arguments[1], p_function_info, r_message);

		} else if (op->op == OP_CALL) {
			if (r_message) {
				*r_message = RTR("Assignment to function.");
			}
			return false;
		}

	} else if (p_node->type == Node::NODE_TYPE_MEMBER) {
		MemberNode *member = static_cast<MemberNode *>(p_node);

		if (member->has_swizzling_duplicates) {
			if (r_message) {
				*r_message = RTR("Swizzling assignment contains duplicates.");
			}
			return false;
		}

		return _validate_assign(member->owner, p_function_info, r_message);

	} else if (p_node->type == Node::NODE_TYPE_VARIABLE) {
		VariableNode *var = static_cast<VariableNode *>(p_node);

		if (shader->uniforms.has(var->name)) {
			if (r_message) {
				*r_message = RTR("Assignment to uniform.");
			}
			return false;
		}

		if (shader->constants.has(var->name) || var->is_const) {
			if (r_message) {
				*r_message = RTR("Constants cannot be modified.");
			}
			return false;
		}

		if (shader->varyings.has(var->name)) {
			return _validate_varying_assign(shader->varyings[var->name], r_message);
		}

		if (!(p_function_info.built_ins.has(var->name) && p_function_info.built_ins[var->name].constant)) {
			return true;
		}
	} else if (p_node->type == Node::NODE_TYPE_ARRAY) {
		ArrayNode *arr = static_cast<ArrayNode *>(p_node);

		if (shader->constants.has(arr->name) || arr->is_const) {
			if (r_message) {
				*r_message = RTR("Constants cannot be modified.");
			}
			return false;
		}

		return true;
	}

	if (r_message) {
		*r_message = "Assignment to constant expression.";
	}
	return false;
}

ShaderLanguage::ShaderNode::Uniform::Hint ShaderLanguage::_sanitize_hint(ShaderNode::Uniform::Hint p_hint) {
	if (p_hint == ShaderNode::Uniform::HINT_SCREEN_TEXTURE ||
			p_hint == ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE ||
			p_hint == ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
		return p_hint;
	}
	return ShaderNode::Uniform::HINT_NONE;
}

bool ShaderLanguage::_propagate_function_call_sampler_uniform_settings(const StringName &p_name, int p_argument, TextureFilter p_filter, TextureRepeat p_repeat, ShaderNode::Uniform::Hint p_hint) {
	for (int i = 0; i < shader->vfunctions.size(); i++) {
		if (shader->vfunctions[i].name == p_name) {
			ERR_FAIL_INDEX_V(p_argument, shader->vfunctions[i].function->arguments.size(), false);
			FunctionNode::Argument *arg = &shader->vfunctions[i].function->arguments.write[p_argument];
			if (arg->tex_builtin_check) {
				_set_error(vformat(RTR("Sampler argument %d of function '%s' called more than once using both built-ins and uniform textures, this is not supported (use either one or the other)."), p_argument, String(p_name)));
				return false;
			} else if (arg->tex_argument_check) {
				// Was checked, verify that filter, repeat, and hint are the same.
				if (arg->tex_argument_filter == p_filter && arg->tex_argument_repeat == p_repeat && arg->tex_hint == _sanitize_hint(p_hint)) {
					return true;
				} else {
					_set_error(vformat(RTR("Sampler argument %d of function '%s' called more than once using textures that differ in either filter, repeat, or texture hint setting."), p_argument, String(p_name)));
					return false;
				}
			} else {
				arg->tex_argument_check = true;
				arg->tex_argument_filter = p_filter;
				arg->tex_argument_repeat = p_repeat;
				arg->tex_hint = _sanitize_hint(p_hint);
				for (KeyValue<StringName, HashSet<int>> &E : arg->tex_argument_connect) {
					for (const int &F : E.value) {
						if (!_propagate_function_call_sampler_uniform_settings(E.key, F, p_filter, p_repeat, p_hint)) {
							return false;
						}
					}
				}
				return true;
			}
		}
	}
	ERR_FAIL_V(false); //bug? function not found
}

bool ShaderLanguage::_propagate_function_call_sampler_builtin_reference(const StringName &p_name, int p_argument, const StringName &p_builtin) {
	for (int i = 0; i < shader->vfunctions.size(); i++) {
		if (shader->vfunctions[i].name == p_name) {
			ERR_FAIL_INDEX_V(p_argument, shader->vfunctions[i].function->arguments.size(), false);
			FunctionNode::Argument *arg = &shader->vfunctions[i].function->arguments.write[p_argument];
			if (arg->tex_argument_check) {
				_set_error(vformat(RTR("Sampler argument %d of function '%s' called more than once using both built-ins and uniform textures, this is not supported (use either one or the other)."), p_argument, String(p_name)));
				return false;
			} else if (arg->tex_builtin_check) {
				//was checked, verify that the built-in is the same
				if (arg->tex_builtin == p_builtin) {
					return true;
				} else {
					_set_error(vformat(RTR("Sampler argument %d of function '%s' called more than once using different built-ins. Only calling with the same built-in is supported."), p_argument, String(p_name)));
					return false;
				}
			} else {
				arg->tex_builtin_check = true;
				arg->tex_builtin = p_builtin;

				for (KeyValue<StringName, HashSet<int>> &E : arg->tex_argument_connect) {
					for (const int &F : E.value) {
						if (!_propagate_function_call_sampler_builtin_reference(E.key, F, p_builtin)) {
							return false;
						}
					}
				}
				return true;
			}
		}
	}
	ERR_FAIL_V(false); //bug? function not found
}

Error ShaderLanguage::_parse_array_size(BlockNode *p_block, const FunctionInfo &p_function_info, bool p_forbid_unknown_size, Node **r_size_expression, int *r_array_size, bool *r_unknown_size) {
	bool error = false;
	if (r_array_size != nullptr && *r_array_size > 0) {
		error = true;
	}
	if (r_unknown_size != nullptr && *r_unknown_size) {
		error = true;
	}
	if (error) {
		_set_error(vformat(RTR("Array size is already defined.")));
		return ERR_PARSE_ERROR;
	}

	TkPos pos = _get_tkpos();
	Token tk = _get_token();

	if (tk.type == TK_BRACKET_CLOSE) {
		if (p_forbid_unknown_size) {
			_set_error(vformat(RTR("Unknown array size is forbidden in that context.")));
			return ERR_PARSE_ERROR;
		}
		if (r_unknown_size != nullptr) {
			*r_unknown_size = true;
		}
	} else {
		_set_tkpos(pos);

		int array_size = 0;
		Node *expr = _parse_and_reduce_expression(p_block, p_function_info);

		if (expr) {
			Vector<Scalar> values = _get_node_values(p_block, expr);

			if (!values.is_empty()) {
				switch (expr->get_datatype()) {
					case TYPE_INT: {
						array_size = values[0].sint;
					} break;
					case TYPE_UINT: {
						array_size = (int)values[0].uint;
					} break;
					default: {
					} break;
				}
			}

			if (r_size_expression != nullptr) {
				*r_size_expression = expr;
			}
		}

		if (array_size <= 0) {
			_set_error(RTR("Expected a positive integer constant."));
			return ERR_PARSE_ERROR;
		}

		tk = _get_token();
		if (tk.type != TK_BRACKET_CLOSE) {
			_set_expected_error("]");
			return ERR_PARSE_ERROR;
		}

		if (r_array_size != nullptr) {
			*r_array_size = array_size;
		}
	}
	return OK;
}

ShaderLanguage::Node *ShaderLanguage::_parse_array_constructor(BlockNode *p_block, const FunctionInfo &p_function_info) {
	DataType type = TYPE_VOID;
	String struct_name = "";
	int array_size = 0;
	bool auto_size = false;
	bool undefined_size = false;
	Token tk = _get_token();

	if (tk.type == TK_CURLY_BRACKET_OPEN) {
		auto_size = true;
	} else {
		if (shader->structs.has(tk.text)) {
			type = TYPE_STRUCT;
			struct_name = tk.text;
		} else {
			if (!is_token_variable_datatype(tk.type)) {
				_set_error(RTR("Invalid data type for the array."));
				return nullptr;
			}
			type = get_token_datatype(tk.type);
		}
		tk = _get_token();
		if (tk.type == TK_BRACKET_OPEN) {
			Error error = _parse_array_size(p_block, p_function_info, false, nullptr, &array_size, &undefined_size);
			if (error != OK) {
				return nullptr;
			}
			tk = _get_token();
		} else {
			_set_expected_error("[");
			return nullptr;
		}
	}

	ArrayConstructNode *an = alloc_node<ArrayConstructNode>();

	if (tk.type == TK_PARENTHESIS_OPEN || auto_size) { // initialization
		int idx = 0;
		while (true) {
			Node *n = _parse_and_reduce_expression(p_block, p_function_info);
			if (!n) {
				return nullptr;
			}

			// define type by using the first member
			if (auto_size && idx == 0) {
				type = n->get_datatype();
				if (type == TYPE_STRUCT) {
					struct_name = n->get_datatype_name();
				}
			} else {
				if (!_compare_datatypes(type, struct_name, 0, n->get_datatype(), n->get_datatype_name(), 0)) {
					return nullptr;
				}
			}

			tk = _get_token();
			if (tk.type == TK_COMMA) {
				an->initializer.push_back(n);
			} else if (!auto_size && tk.type == TK_PARENTHESIS_CLOSE) {
				an->initializer.push_back(n);
				break;
			} else if (auto_size && tk.type == TK_CURLY_BRACKET_CLOSE) {
				an->initializer.push_back(n);
				break;
			} else {
				if (auto_size) {
					_set_expected_error("}", ",");
				} else {
					_set_expected_error(")", ",");
				}
				return nullptr;
			}
			idx++;
		}
		if (!auto_size && !undefined_size && an->initializer.size() != array_size) {
			_set_error(vformat(RTR("Array size mismatch. Expected %d elements (found %d)."), array_size, an->initializer.size()));
			return nullptr;
		}
	} else {
		_set_error(RTR("Expected array initialization."));
		return nullptr;
	}

	an->datatype = type;
	an->struct_name = struct_name;
	return an;
}

ShaderLanguage::Node *ShaderLanguage::_parse_array_constructor(BlockNode *p_block, const FunctionInfo &p_function_info, DataType p_type, const StringName &p_struct_name, int p_array_size) {
	DataType type = TYPE_VOID;
	String struct_name = "";
	int array_size = 0;
	bool auto_size = false;
	TkPos prev_pos = _get_tkpos();
	Token tk = _get_token();

	if (tk.type == TK_CURLY_BRACKET_OPEN) {
		auto_size = true;
	} else {
		if (shader->structs.has(tk.text)) {
			type = TYPE_STRUCT;
			struct_name = tk.text;
		} else {
			if (!is_token_variable_datatype(tk.type)) {
				_set_tkpos(prev_pos);

				Node *n = _parse_and_reduce_expression(p_block, p_function_info);

				if (!n) {
					_set_error(RTR("Invalid data type for the array."));
					return nullptr;
				}

				if (!_compare_datatypes(p_type, p_struct_name, p_array_size, n->get_datatype(), n->get_datatype_name(), n->get_array_size())) {
					return nullptr;
				}
				return n;
			}
			type = get_token_datatype(tk.type);
		}
		tk = _get_token();
		if (tk.type == TK_BRACKET_OPEN) {
			bool is_unknown_size = false;
			Error error = _parse_array_size(p_block, p_function_info, false, nullptr, &array_size, &is_unknown_size);
			if (error != OK) {
				return nullptr;
			}
			if (is_unknown_size) {
				array_size = p_array_size;
			}
			tk = _get_token();
		} else {
			_set_expected_error("[");
			return nullptr;
		}

		if (type != p_type || struct_name != p_struct_name || array_size != p_array_size) {
			String from;
			if (type == TYPE_STRUCT) {
				from += struct_name;
			} else {
				from += get_datatype_name(type);
			}
			from += "[";
			from += itos(array_size);
			from += "]'";

			String to;
			if (type == TYPE_STRUCT) {
				to += p_struct_name;
			} else {
				to += get_datatype_name(p_type);
			}
			to += "[";
			to += itos(p_array_size);
			to += "]'";

			_set_error(vformat(RTR("Cannot convert from '%s' to '%s'."), from, to));
			return nullptr;
		}
	}

	ArrayConstructNode *an = alloc_node<ArrayConstructNode>();
	an->datatype = p_type;
	an->struct_name = p_struct_name;

	if (tk.type == TK_PARENTHESIS_OPEN || auto_size) { // initialization
		while (true) {
			Node *n = _parse_and_reduce_expression(p_block, p_function_info);
			if (!n) {
				return nullptr;
			}

			if (!_compare_datatypes(p_type, p_struct_name, 0, n->get_datatype(), n->get_datatype_name(), n->get_array_size())) {
				return nullptr;
			}

			tk = _get_token();
			if (tk.type == TK_COMMA) {
				an->initializer.push_back(n);
			} else if (!auto_size && tk.type == TK_PARENTHESIS_CLOSE) {
				an->initializer.push_back(n);
				break;
			} else if (auto_size && tk.type == TK_CURLY_BRACKET_CLOSE) {
				an->initializer.push_back(n);
				break;
			} else {
				if (auto_size) {
					_set_expected_error("}", ",");
				} else {
					_set_expected_error(")", ",");
				}
				return nullptr;
			}
		}
		if (an->initializer.size() != p_array_size) {
			_set_error(vformat(RTR("Array size mismatch. Expected %d elements (found %d)."), p_array_size, an->initializer.size()));
			return nullptr;
		}
	} else {
		_set_error(RTR("Expected array initialization."));
		return nullptr;
	}

	return an;
}

ShaderLanguage::Node *ShaderLanguage::_parse_expression(BlockNode *p_block, const FunctionInfo &p_function_info, const ExpressionInfo *p_previous_expression_info) {
	Vector<Expression> expression;

	//Vector<TokenType> operators;
#ifdef DEBUG_ENABLED
	bool check_position_write = check_warnings && HAS_WARNING(ShaderWarning::MAGIC_POSITION_WRITE_FLAG);
	check_position_write = check_position_write && String(shader_type_identifier) == "spatial" && current_function == "vertex";
#endif

	while (true) {
		Node *expr = nullptr;
		TkPos prepos = _get_tkpos();
		Token tk = _get_token();
		TkPos pos = _get_tkpos();

		bool is_const = false;

		if (tk.type == TK_PARENTHESIS_OPEN) {
			//handle subexpression

			expr = _parse_and_reduce_expression(p_block, p_function_info);
			if (!expr) {
				return nullptr;
			}

			tk = _get_token();

			if (tk.type != TK_PARENTHESIS_CLOSE) {
				_set_error(RTR("Expected ')' in expression."));
				return nullptr;
			}

		} else if (tk.type == TK_FLOAT_CONSTANT) {
			ConstantNode *constant = alloc_node<ConstantNode>();
			Scalar v;
			v.real = tk.constant;
			constant->values.push_back(v);
			constant->datatype = TYPE_FLOAT;
			expr = constant;

		} else if (tk.type == TK_INT_CONSTANT) {
			ConstantNode *constant = alloc_node<ConstantNode>();
			Scalar v;
			v.sint = tk.constant;
			constant->values.push_back(v);
			constant->datatype = TYPE_INT;
			expr = constant;

		} else if (tk.type == TK_UINT_CONSTANT) {
			ConstantNode *constant = alloc_node<ConstantNode>();
			Scalar v;
			v.uint = tk.constant;
			constant->values.push_back(v);
			constant->datatype = TYPE_UINT;
			expr = constant;

		} else if (tk.type == TK_TRUE) {
			//handle true constant
			ConstantNode *constant = alloc_node<ConstantNode>();
			Scalar v;
			v.boolean = true;
			constant->values.push_back(v);
			constant->datatype = TYPE_BOOL;
			expr = constant;

		} else if (tk.type == TK_FALSE) {
			//handle false constant
			ConstantNode *constant = alloc_node<ConstantNode>();
			Scalar v;
			v.boolean = false;
			constant->values.push_back(v);
			constant->datatype = TYPE_BOOL;
			expr = constant;

		} else if (tk.type == TK_TYPE_VOID) {
			//make sure void is not used in expression
			_set_error(RTR("Void value not allowed in expression."));
			return nullptr;
		} else if (is_token_nonvoid_datatype(tk.type) || tk.type == TK_CURLY_BRACKET_OPEN) {
			if (tk.type == TK_CURLY_BRACKET_OPEN) {
				//array constructor

				_set_tkpos(prepos);
				expr = _parse_array_constructor(p_block, p_function_info);
			} else {
				DataType datatype;
				DataPrecision precision = PRECISION_DEFAULT;

				if (is_token_precision(tk.type)) {
					precision = get_token_precision(tk.type);
					tk = _get_token();
				}

				datatype = get_token_datatype(tk.type);
				if (precision != PRECISION_DEFAULT && _validate_precision(datatype, precision) != OK) {
					return nullptr;
				}

				tk = _get_token();

				if (tk.type == TK_BRACKET_OPEN) {
					//array constructor

					_set_tkpos(prepos);
					expr = _parse_array_constructor(p_block, p_function_info);
				} else {
					if (tk.type != TK_PARENTHESIS_OPEN) {
						_set_error(RTR("Expected '(' after the type name."));
						return nullptr;
					}
					//basic type constructor

					OperatorNode *func = alloc_node<OperatorNode>();
					func->op = OP_CONSTRUCT;

					if (precision != PRECISION_DEFAULT) {
						func->return_precision_cache = precision;
					}

					VariableNode *funcname = alloc_node<VariableNode>();
					funcname->name = get_datatype_name(datatype);
					func->arguments.push_back(funcname);

					int carg = -1;

					bool ok = _parse_function_arguments(p_block, p_function_info, func, &carg);

					if (carg >= 0) {
						completion_type = COMPLETION_CALL_ARGUMENTS;
						completion_line = tk_line;
						completion_block = p_block;
						completion_function = funcname->name;
						completion_argument = carg;
					}

					if (!ok) {
						return nullptr;
					}

					if (!_validate_function_call(p_block, p_function_info, func, &func->return_cache, &func->struct_name)) {
						_set_error(vformat(RTR("No matching constructor found for: '%s'."), String(funcname->name)));
						return nullptr;
					}

					expr = _reduce_expression(p_block, func);
				}
			}
		} else if (tk.type == TK_IDENTIFIER) {
			_set_tkpos(prepos);

			StringName identifier;

			StructNode *pstruct = nullptr;
			bool struct_init = false;

			_get_completable_identifier(p_block, COMPLETION_IDENTIFIER, identifier);

			if (shader->structs.has(identifier)) {
				pstruct = shader->structs[identifier].shader_struct;
				struct_init = true;
			}

			tk = _get_token();
			if (tk.type == TK_PARENTHESIS_OPEN) {
				if (struct_init) { //a struct constructor

					const StringName &name = identifier;

					OperatorNode *func = alloc_node<OperatorNode>();
					func->op = OP_STRUCT;
					func->struct_name = name;
					func->return_cache = TYPE_STRUCT;
					VariableNode *funcname = alloc_node<VariableNode>();
					funcname->name = name;
					func->arguments.push_back(funcname);

					for (List<ShaderLanguage::MemberNode *>::Element *E = pstruct->members.front(); E; E = E->next()) {
						Node *nexpr;

						if (E->get()->array_size != 0) {
							nexpr = _parse_array_constructor(p_block, p_function_info, E->get()->get_datatype(), E->get()->struct_name, E->get()->array_size);
							if (!nexpr) {
								return nullptr;
							}
						} else {
							nexpr = _parse_and_reduce_expression(p_block, p_function_info);
							if (!nexpr) {
								return nullptr;
							}
							if (!_compare_datatypes_in_nodes(E->get(), nexpr)) {
								return nullptr;
							}
						}

						if (E->next()) {
							tk = _get_token();
							if (tk.type != TK_COMMA) {
								_set_expected_error(",");
								return nullptr;
							}
						}
						func->arguments.push_back(nexpr);
					}
					tk = _get_token();
					if (tk.type != TK_PARENTHESIS_CLOSE) {
						_set_expected_error(")");
						return nullptr;
					}

					expr = func;

				} else { //a function call

					// Non-builtin function call is forbidden for constant declaration.
					if (is_const_decl) {
						if (shader->functions.has(identifier)) {
							_set_error(RTR("Expected constant expression."));
							return nullptr;
						}
					}

					const StringName &rname = identifier;
					StringName name = identifier;

					OperatorNode *func = alloc_node<OperatorNode>();
					func->op = OP_CALL;

					VariableNode *funcname = alloc_node<VariableNode>();
					funcname->name = name;
					funcname->rname = name;

					func->arguments.push_back(funcname);

					int carg = -1;

					bool ok = _parse_function_arguments(p_block, p_function_info, func, &carg);

					// Check if block has a variable with the same name as function to prevent shader crash.
					ShaderLanguage::BlockNode *bnode = p_block;
					while (bnode) {
						if (bnode->variables.has(name)) {
							_set_error(RTR("Expected a function name."));
							return nullptr;
						}
						bnode = bnode->parent_block;
					}

					// Test if function was parsed first.
					int function_index = -1;
					for (int i = 0, max_valid_args = 0; i < shader->vfunctions.size(); i++) {
						if (!shader->vfunctions[i].callable || shader->vfunctions[i].rname != rname) {
							continue;
						}

						bool found = true;
						int valid_args = 0;

						// Search for correct overload.
						for (int j = 1; j < func->arguments.size(); j++) {
							if (j - 1 == shader->vfunctions[i].function->arguments.size()) {
								found = false;
								break;
							}

							const FunctionNode::Argument &a = shader->vfunctions[i].function->arguments[j - 1];
							Node *b = func->arguments[j];

							if (a.type == b->get_datatype() && a.array_size == b->get_array_size()) {
								if (a.type == TYPE_STRUCT) {
									if (a.struct_name != b->get_datatype_name()) {
										found = false;
										break;
									} else {
										valid_args++;
									}
								} else {
									valid_args++;
								}
							} else {
								if (function_overload_count[rname] == 0 && get_scalar_type(a.type) == a.type && b->type == Node::NODE_TYPE_CONSTANT && a.array_size == 0 && convert_constant(static_cast<ConstantNode *>(b), a.type)) {
									// Implicit cast if no overloads.
									continue;
								}
								found = false;
								break;
							}
						}

						// Using the best match index for completion hint if the function not found.
						if (valid_args > max_valid_args) {
							name = shader->vfunctions[i].name;
							funcname->name = name;
							max_valid_args = valid_args;
						}

						if (!found) {
							continue;
						}

						// Add to current function as dependency.
						for (int j = 0; j < shader->vfunctions.size(); j++) {
							if (shader->vfunctions[j].name == current_function) {
								shader->vfunctions.write[j].uses_function.insert(name);
								break;
							}
						}

						name = shader->vfunctions[i].name;
						funcname->name = name;

						// See if texture arguments must connect.
						function_index = i;
						break;
					}

					if (carg >= 0) {
						completion_type = COMPLETION_CALL_ARGUMENTS;
						completion_line = tk_line;
						completion_block = p_block;
						completion_function = funcname->name;
						completion_argument = carg;
					}

					if (!ok) {
						return nullptr;
					}

					if (name != current_function) { // Recursion is not allowed.
						// Register call.
						if (calls_info.has(name)) {
							calls_info[current_function].calls.push_back(&calls_info[name]);
						}

						bool is_builtin = false;

						if (is_supported_frag_only_funcs && stages) {
							for (const KeyValue<StringName, FunctionInfo> &E : *stages) {
								if (E.value.stage_functions.has(name)) {
									// Register usage of the restricted stage function.
									calls_info[current_function].uses_restricted_items.push_back(Pair<StringName, CallInfo::Item>(name, CallInfo::Item(CallInfo::Item::ITEM_TYPE_BUILTIN, _get_tkpos())));
									is_builtin = true;
									break;
								}
							}
						}

						if (!is_builtin) {
							int idx = 0;
							while (frag_only_func_defs[idx].name) {
								if (frag_only_func_defs[idx].name == name) {
									// If a built-in function not found for the current shader type, then it shouldn't be parsed further.
									if (!is_supported_frag_only_funcs) {
										_set_error(vformat(RTR("Built-in function '%s' is not supported for the '%s' shader type."), name, shader_type_identifier));
										return nullptr;
									}
									// Register usage of the restricted function.
									calls_info[current_function].uses_restricted_items.push_back(Pair<StringName, CallInfo::Item>(name, CallInfo::Item(CallInfo::Item::ITEM_TYPE_BUILTIN, _get_tkpos())));
									is_builtin = true;
									break;
								}
								idx++;
							}
						}

						// Recursively checks for the restricted function call.
						if (is_supported_frag_only_funcs && current_function == "vertex" && stages->has(current_function) && !_validate_restricted_func(name, &calls_info[current_function], is_builtin)) {
							return nullptr;
						}
					}

					bool is_custom_func = false;
					if (!_validate_function_call(p_block, p_function_info, func, &func->return_cache, &func->struct_name, &is_custom_func)) {
						_set_error(vformat(RTR("No matching function found for: '%s'."), String(funcname->rname)));
						return nullptr;
					}
					completion_class = TAG_GLOBAL; // reset sub-class
					if (function_index >= 0) {
						//connect texture arguments, so we can cache in the
						//argument what type of filter and repeat to use

						FunctionNode *call_function = shader->vfunctions[function_index].function;
						if (call_function) {
							func->return_cache = call_function->get_datatype();
							func->struct_name = call_function->get_datatype_name();
							func->return_array_size = call_function->get_array_size();

							//get current base function
							FunctionNode *base_function = nullptr;
							{
								BlockNode *b = p_block;

								while (b) {
									if (b->parent_function) {
										base_function = b->parent_function;
										break;
									} else {
										b = b->parent_block;
									}
								}
							}

							ERR_FAIL_NULL_V(base_function, nullptr); // Bug, wtf.

							for (int i = 0; i < call_function->arguments.size(); i++) {
								int argidx = i + 1;
								if (argidx < func->arguments.size()) {
									bool error = false;
									Node *n = func->arguments[argidx];
									ArgumentQualifier arg_qual = call_function->arguments[i].qualifier;
									bool is_out_arg = arg_qual != ArgumentQualifier::ARGUMENT_QUALIFIER_IN;

									if (n->type == Node::NODE_TYPE_VARIABLE || n->type == Node::NODE_TYPE_ARRAY) {
										StringName varname;

										if (n->type == Node::NODE_TYPE_VARIABLE) {
											VariableNode *vn = static_cast<VariableNode *>(n);
											varname = vn->name;
										} else { // TYPE_ARRAY
											ArrayNode *an = static_cast<ArrayNode *>(n);
											varname = an->name;
										}

										if (shader->varyings.has(varname)) {
											switch (shader->varyings[varname].stage) {
												case ShaderNode::Varying::STAGE_UNKNOWN:
													if (is_out_arg) {
														error = true;
													}
													break;
												case ShaderNode::Varying::STAGE_VERTEX_TO_FRAGMENT_LIGHT:
													[[fallthrough]];
												case ShaderNode::Varying::STAGE_VERTEX:
													if (is_out_arg && current_function != varying_function_names.vertex) { // inout/out
														error = true;
													}
													break;
												case ShaderNode::Varying::STAGE_FRAGMENT_TO_LIGHT:
													[[fallthrough]];
												case ShaderNode::Varying::STAGE_FRAGMENT:
													if (!is_out_arg) {
														if (current_function != varying_function_names.fragment && current_function != varying_function_names.light) {
															error = true;
														}
													} else if (current_function != varying_function_names.fragment) { // inout/out
														error = true;
													}
													break;
												default:
													break;
											}

											if (error) {
												_set_error(vformat(RTR("Varying '%s' cannot be passed for the '%s' parameter in that context."), varname, _get_qualifier_str(arg_qual)));
												return nullptr;
											}
										}
									}

									bool is_const_arg = call_function->arguments[i].is_const;

									if (is_const_arg || is_out_arg) {
										StringName varname;

										if (n->type == Node::NODE_TYPE_CONSTANT || n->type == Node::NODE_TYPE_OPERATOR || n->type == Node::NODE_TYPE_ARRAY_CONSTRUCT) {
											if (!is_const_arg) {
												error = true;
											}
										} else if (n->type == Node::NODE_TYPE_ARRAY) {
											ArrayNode *an = static_cast<ArrayNode *>(n);
											if (!is_const_arg && (an->call_expression != nullptr || an->is_const)) {
												error = true;
											}
											varname = an->name;
										} else if (n->type == Node::NODE_TYPE_VARIABLE) {
											VariableNode *vn = static_cast<VariableNode *>(n);
											if (vn->is_const && !is_const_arg) {
												error = true;
											}
											varname = vn->name;
										} else if (n->type == Node::NODE_TYPE_MEMBER) {
											MemberNode *mn = static_cast<MemberNode *>(n);
											if (mn->basetype_const && is_out_arg) {
												error = true;
											}
										}
										if (!error && varname != StringName()) {
											if (shader->constants.has(varname)) {
												error = true;
											} else if (shader->uniforms.has(varname)) {
												error = true;
											} else if (p_function_info.built_ins.has(varname)) {
												BuiltInInfo info = p_function_info.built_ins[varname];
												if (info.constant) {
													error = true;
												}
											}
										}

										if (error) {
											_set_error(vformat(RTR("A constant value cannot be passed for the '%s' parameter."), _get_qualifier_str(arg_qual)));
											return nullptr;
										}
									}
									if (is_sampler_type(call_function->arguments[i].type)) {
										// Let's see where our argument comes from.
										StringName varname;
										if (n->type == Node::NODE_TYPE_VARIABLE) {
											VariableNode *vn = static_cast<VariableNode *>(n);
											varname = vn->name;
										} else if (n->type == Node::NODE_TYPE_ARRAY) {
											ArrayNode *an = static_cast<ArrayNode *>(n);
											varname = an->name;
										}

										if (shader->uniforms.has(varname)) {
											//being sampler, this either comes from a uniform
											ShaderNode::Uniform *u = &shader->uniforms[varname];
											ERR_CONTINUE(u->type != call_function->arguments[i].type); //this should have been validated previously

											if (RendererCompositor::get_singleton()->is_xr_enabled() && is_custom_func) {
												ShaderNode::Uniform::Hint hint = u->hint;

												if (hint == ShaderNode::Uniform::HINT_DEPTH_TEXTURE || hint == ShaderNode::Uniform::HINT_SCREEN_TEXTURE || hint == ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE) {
													_set_error(vformat(RTR("Unable to pass a multiview texture sampler as a parameter to custom function. Consider to sample it in the main function and then pass the vector result to it."), get_uniform_hint_name(hint)));
													return nullptr;
												}
											}

											//propagate
											if (!_propagate_function_call_sampler_uniform_settings(name, i, u->filter, u->repeat, u->hint)) {
												return nullptr;
											}
										} else if (p_function_info.built_ins.has(varname)) {
											//a built-in
											if (!_propagate_function_call_sampler_builtin_reference(name, i, varname)) {
												return nullptr;
											}
										} else {
											//or this comes from an argument, but nothing else can be a sampler
											bool found = false;
											for (int j = 0; j < base_function->arguments.size(); j++) {
												if (base_function->arguments[j].name == varname) {
													if (!base_function->arguments[j].tex_argument_connect.has(call_function->name)) {
														base_function->arguments.write[j].tex_argument_connect[call_function->name] = HashSet<int>();
													}
													base_function->arguments.write[j].tex_argument_connect[call_function->name].insert(i);
													found = true;
													break;
												}
											}
											ERR_CONTINUE(!found);
										}
									}
								} else {
									break;
								}
							}
						}
					}
					expr = func;
#ifdef DEBUG_ENABLED
					if (check_warnings && is_custom_func) {
						StringName func_name;

						if (p_block && p_block->parent_function) {
							func_name = p_block->parent_function->name;
						}

						_parse_used_identifier(name, IdentifierType::IDENTIFIER_FUNCTION, func_name);
					}
#endif // DEBUG_ENABLED
				}
			} else {
				//an identifier

				last_name = identifier;
				last_type = IDENTIFIER_MAX;
				_set_tkpos(pos);

				DataType data_type = TYPE_MAX;
				IdentifierType ident_type = IDENTIFIER_MAX;
				int array_size = 0;
				StringName struct_name;
				bool is_local = false;

				if (p_block && p_block->block_tag != SubClassTag::TAG_GLOBAL) {
					int idx = 0;
					bool found = false;

					while (builtin_func_defs[idx].name) {
						if (builtin_func_defs[idx].tag == p_block->block_tag && builtin_func_defs[idx].name == identifier) {
							found = true;
							break;
						}
						idx++;
					}
					if (!found) {
						_set_error(vformat(RTR("Unknown identifier in expression: '%s'."), String(identifier)));
						return nullptr;
					}
				} else {
					if (!_find_identifier(p_block, false, p_function_info, identifier, &data_type, &ident_type, &is_const, &array_size, &struct_name)) {
						if (identifier == "SCREEN_TEXTURE" || identifier == "DEPTH_TEXTURE" || identifier == "NORMAL_ROUGHNESS_TEXTURE") {
							String name = String(identifier);
							String name_lower = name.to_lower();
							_set_error(vformat(RTR("%s has been removed in favor of using hint_%s with a uniform.\nTo continue with minimal code changes add 'uniform sampler2D %s : hint_%s, filter_linear_mipmap;' near the top of your shader."), name, name_lower, name, name_lower));
							return nullptr;
						}
						_set_error(vformat(RTR("Unknown identifier in expression: '%s'."), String(identifier)));
						return nullptr;
					}
					if (is_const_decl && !is_const) {
						_set_error(RTR("Expected constant expression."));
						return nullptr;
					}
					if (ident_type == IDENTIFIER_VARYING) {
						TkPos prev_pos = _get_tkpos();
						Token next_token = _get_token();

						// An array of varyings.
						if (next_token.type == TK_BRACKET_OPEN) {
							_get_token(); // Pass constant.
							_get_token(); // Pass TK_BRACKET_CLOSE.
							next_token = _get_token();
						}
						_set_tkpos(prev_pos);

						ShaderNode::Varying &var = shader->varyings[identifier];
						calls_info[current_function].uses_restricted_items.push_back(Pair<StringName, CallInfo::Item>(identifier, CallInfo::Item(CallInfo::Item::ITEM_TYPE_VARYING, prev_pos)));

						String error;
						if (is_token_operator_assign(next_token.type)) {
							if (!_validate_varying_assign(shader->varyings[identifier], &error)) {
								_set_error(error);
								return nullptr;
							}
						} else {
							switch (var.stage) {
								case ShaderNode::Varying::STAGE_UNKNOWN: {
									if (var.type < TYPE_INT) {
										if (current_function == varying_function_names.vertex) {
											_set_error(vformat(RTR("Varying with '%s' data type may only be used in the 'fragment' function."), get_datatype_name(var.type)));
										} else {
											_set_error(vformat(RTR("Varying '%s' must be assigned in the 'fragment' function first."), identifier));
										}
										return nullptr;
									}
								} break;
								case ShaderNode::Varying::STAGE_VERTEX:
									if (current_function == varying_function_names.fragment || current_function == varying_function_names.light) {
										var.stage = ShaderNode::Varying::STAGE_VERTEX_TO_FRAGMENT_LIGHT;
									}
									break;
								case ShaderNode::Varying::STAGE_FRAGMENT:
									if (current_function == varying_function_names.light) {
										var.stage = ShaderNode::Varying::STAGE_FRAGMENT_TO_LIGHT;
									}
									break;
								default:
									break;
							}
						}

						if ((var.stage != ShaderNode::Varying::STAGE_FRAGMENT && var.stage != ShaderNode::Varying::STAGE_FRAGMENT_TO_LIGHT) && var.type < TYPE_FLOAT && var.interpolation != INTERPOLATION_FLAT) {
							_set_tkpos(var.tkpos);
							_set_error(RTR("Varying with integer data type must be declared with `flat` interpolation qualifier."));
							return nullptr;
						}
					}

					if (ident_type == IDENTIFIER_FUNCTION) {
						_set_error(vformat(RTR("Can't use function as identifier: '%s'."), String(identifier)));
						return nullptr;
					}
#ifdef DEBUG_ENABLED
					if (check_position_write && ident_type == IDENTIFIER_BUILTIN_VAR) {
						if (String(identifier) == "POSITION") {
							// Check if the user wrote "POSITION = vec4(VERTEX," and warn if they did.
							TkPos prev_pos = _get_tkpos();
							if (_get_token().type == TK_OP_ASSIGN &&
									_get_token().type == TK_TYPE_VEC4 &&
									_get_token().type == TK_PARENTHESIS_OPEN &&
									_get_token().text == "VERTEX" &&
									_get_token().type == TK_COMMA) {
								_add_line_warning(ShaderWarning::MAGIC_POSITION_WRITE);
							}

							// Reset the position so compiling can continue as normal.
							_set_tkpos(prev_pos);
						}
					}
#endif
					if (is_const) {
						last_type = IDENTIFIER_CONSTANT;
					} else {
						last_type = ident_type;
					}

					is_local = ident_type == IDENTIFIER_LOCAL_VAR || ident_type == IDENTIFIER_FUNCTION_ARGUMENT;
				}

				Node *index_expression = nullptr;
				Node *call_expression = nullptr;
				Node *assign_expression = nullptr;

				if (array_size > 0) {
					prepos = _get_tkpos();
					tk = _get_token();

					if (tk.type == TK_OP_ASSIGN) {
						if (is_const) {
							_set_error(RTR("Constants cannot be modified."));
							return nullptr;
						}
						assign_expression = _parse_array_constructor(p_block, p_function_info, data_type, struct_name, array_size);
						if (!assign_expression) {
							return nullptr;
						}
					} else if (tk.type == TK_PERIOD) {
						completion_class = TAG_ARRAY;
						if (p_block != nullptr) {
							p_block->block_tag = SubClassTag::TAG_ARRAY;
						}
						call_expression = _parse_and_reduce_expression(p_block, p_function_info);
						if (p_block != nullptr) {
							p_block->block_tag = SubClassTag::TAG_GLOBAL;
						}
						if (!call_expression) {
							return nullptr;
						}
					} else if (tk.type == TK_BRACKET_OPEN) { // indexing
						index_expression = _parse_and_reduce_expression(p_block, p_function_info);
						if (!index_expression) {
							return nullptr;
						}

						if (index_expression->get_array_size() != 0 || (index_expression->get_datatype() != TYPE_INT && index_expression->get_datatype() != TYPE_UINT)) {
							_set_error(RTR("Only integer expressions are allowed for indexing."));
							return nullptr;
						}

						if (index_expression->type == Node::NODE_TYPE_CONSTANT) {
							ConstantNode *cnode = static_cast<ConstantNode *>(index_expression);
							if (cnode) {
								if (!cnode->values.is_empty()) {
									int value = cnode->values[0].sint;
									if (value < 0 || value >= array_size) {
										_set_error(vformat(RTR("Index [%d] out of range [%d..%d]."), value, 0, array_size - 1));
										return nullptr;
									}
								}
							}
						}

						tk = _get_token();
						if (tk.type != TK_BRACKET_CLOSE) {
							_set_expected_error("]");
							return nullptr;
						}
					} else {
						_set_tkpos(prepos);
					}

					ArrayNode *arrname = alloc_node<ArrayNode>();
					arrname->name = identifier;
					arrname->datatype_cache = data_type;
					arrname->struct_name = struct_name;
					arrname->index_expression = index_expression;
					arrname->call_expression = call_expression;
					arrname->assign_expression = assign_expression;
					arrname->is_const = is_const;
					arrname->array_size = array_size;
					arrname->is_local = is_local;
					expr = arrname;
				} else {
					VariableNode *varname = alloc_node<VariableNode>();
					varname->name = identifier;
					varname->datatype_cache = data_type;
					varname->is_const = is_const;
					varname->struct_name = struct_name;
					varname->is_local = is_local;
					expr = varname;
				}
#ifdef DEBUG_ENABLED
				if (check_warnings) {
					StringName func_name;
					BlockNode *b = p_block;

					while (b) {
						if (b->parent_function) {
							func_name = b->parent_function->name;
							break;
						} else {
							b = b->parent_block;
						}
					}

					_parse_used_identifier(identifier, ident_type, func_name);
				}
#endif // DEBUG_ENABLED
			}
		} else if (tk.type == TK_OP_ADD) {
			continue; //this one does nothing
		} else if (tk.type == TK_OP_SUB || tk.type == TK_OP_NOT || tk.type == TK_OP_BIT_INVERT || tk.type == TK_OP_INCREMENT || tk.type == TK_OP_DECREMENT) {
			Expression e;
			e.is_op = true;

			switch (tk.type) {
				case TK_OP_SUB:
					e.op = OP_NEGATE;
					break;
				case TK_OP_NOT:
					e.op = OP_NOT;
					break;
				case TK_OP_BIT_INVERT:
					e.op = OP_BIT_INVERT;
					break;
				case TK_OP_INCREMENT:
					e.op = OP_INCREMENT;
					break;
				case TK_OP_DECREMENT:
					e.op = OP_DECREMENT;
					break;
				default:
					ERR_FAIL_V(nullptr);
			}

			expression.push_back(e);
			continue;
		} else {
			bool valid = false;
			if (p_block && p_block->block_type == BlockNode::BLOCK_TYPE_FOR_EXPRESSION && tk.type == TK_PARENTHESIS_CLOSE) {
				valid = true;
				_set_tkpos(prepos);

				OperatorNode *func = alloc_node<OperatorNode>();
				func->op = OP_EMPTY;
				expr = func;
			}
			if (!valid) {
				if (tk.type != TK_SEMICOLON) {
					_set_error(vformat(RTR("Expected expression, found: '%s'."), get_token_text(tk)));
					return nullptr;
				} else {
#ifdef DEBUG_ENABLED
					if (!p_block || (p_block->block_type != BlockNode::BLOCK_TYPE_FOR_INIT && p_block->block_type != BlockNode::BLOCK_TYPE_FOR_CONDITION)) {
						if (check_warnings && HAS_WARNING(ShaderWarning::FORMATTING_ERROR_FLAG)) {
							_add_line_warning(ShaderWarning::FORMATTING_ERROR, RTR("Empty statement. Remove ';' to fix this warning."));
						}
					}
#endif // DEBUG_ENABLED
					_set_tkpos(prepos);

					OperatorNode *func = alloc_node<OperatorNode>();
					func->op = OP_EMPTY;
					expr = func;
				}
			}
		}

		ERR_FAIL_NULL_V(expr, nullptr);

		/* OK now see what's NEXT to the operator.. */

		while (true) {
			TkPos pos2 = _get_tkpos();
			tk = _get_token();

			if (tk.type == TK_CURSOR) {
				//do nothing
			} else if (tk.type == TK_PERIOD) {
#ifdef DEBUG_ENABLED
				uint32_t prev_keyword_completion_context = keyword_completion_context;
				keyword_completion_context = CF_UNSPECIFIED;
#endif

				DataType dt = expr->get_datatype();
				String st = expr->get_datatype_name();

				if (!expr->is_indexed() && expr->get_array_size() > 0) {
					completion_class = TAG_ARRAY;
					if (p_block != nullptr) {
						p_block->block_tag = SubClassTag::TAG_ARRAY;
					}
					Node *call_expression = _parse_and_reduce_expression(p_block, p_function_info);
					if (p_block != nullptr) {
						p_block->block_tag = SubClassTag::TAG_GLOBAL;
					}
					if (!call_expression) {
						return nullptr;
					}
					expr = call_expression;
					break;
				}

				StringName identifier;
				if (_get_completable_identifier(p_block, dt == TYPE_STRUCT ? COMPLETION_STRUCT : COMPLETION_INDEX, identifier)) {
					if (dt == TYPE_STRUCT) {
						completion_struct = st;
					} else {
						completion_base = dt;
					}
				}

				if (identifier == StringName()) {
					_set_error(RTR("Expected an identifier as a member."));
					return nullptr;
				}
				String ident = identifier;

				bool ok = true;
				bool repeated = false;
				DataType member_type = TYPE_VOID;
				StringName member_struct_name = "";
				int array_size = 0;

				RBSet<char> position_symbols;
				RBSet<char> color_symbols;
				RBSet<char> texture_symbols;

				bool mix_error = false;

				switch (dt) {
					case TYPE_STRUCT: {
						ok = false;
						String member_name = String(ident.ptr());
						if (shader->structs.has(st)) {
							StructNode *n = shader->structs[st].shader_struct;
							for (const MemberNode *E : n->members) {
								if (String(E->name) == member_name) {
									member_type = E->datatype;
									array_size = E->array_size;
									if (member_type == TYPE_STRUCT) {
										member_struct_name = E->struct_name;
									}
									ok = true;
									break;
								}
							}
						}

					} break;
					case TYPE_BVEC2:
					case TYPE_IVEC2:
					case TYPE_UVEC2:
					case TYPE_VEC2: {
						int l = ident.length();
						if (l == 1) {
							member_type = DataType(dt - 1);
						} else if (l == 2) {
							member_type = dt;
						} else if (l == 3) {
							member_type = DataType(dt + 1);
						} else if (l == 4) {
							member_type = DataType(dt + 2);
						} else {
							ok = false;
							break;
						}

						const char32_t *c = ident.ptr();
						for (int i = 0; i < l; i++) {
							switch (c[i]) {
								case 'r':
								case 'g':
									if (position_symbols.size() > 0 || texture_symbols.size() > 0) {
										mix_error = true;
										break;
									}
									if (!color_symbols.has(c[i])) {
										color_symbols.insert(c[i]);
									} else {
										repeated = true;
									}
									break;
								case 'x':
								case 'y':
									if (color_symbols.size() > 0 || texture_symbols.size() > 0) {
										mix_error = true;
										break;
									}
									if (!position_symbols.has(c[i])) {
										position_symbols.insert(c[i]);
									} else {
										repeated = true;
									}
									break;
								case 's':
								case 't':
									if (color_symbols.size() > 0 || position_symbols.size() > 0) {
										mix_error = true;
										break;
									}
									if (!texture_symbols.has(c[i])) {
										texture_symbols.insert(c[i]);
									} else {
										repeated = true;
									}
									break;
								default:
									ok = false;
									break;
							}
						}

					} break;
					case TYPE_BVEC3:
					case TYPE_IVEC3:
					case TYPE_UVEC3:
					case TYPE_VEC3: {
						int l = ident.length();
						if (l == 1) {
							member_type = DataType(dt - 2);
						} else if (l == 2) {
							member_type = DataType(dt - 1);
						} else if (l == 3) {
							member_type = dt;
						} else if (l == 4) {
							member_type = DataType(dt + 1);
						} else {
							ok = false;
							break;
						}

						const char32_t *c = ident.ptr();
						for (int i = 0; i < l; i++) {
							switch (c[i]) {
								case 'r':
								case 'g':
								case 'b':
									if (position_symbols.size() > 0 || texture_symbols.size() > 0) {
										mix_error = true;
										break;
									}
									if (!color_symbols.has(c[i])) {
										color_symbols.insert(c[i]);
									} else {
										repeated = true;
									}
									break;
								case 'x':
								case 'y':
								case 'z':
									if (color_symbols.size() > 0 || texture_symbols.size() > 0) {
										mix_error = true;
										break;
									}
									if (!position_symbols.has(c[i])) {
										position_symbols.insert(c[i]);
									} else {
										repeated = true;
									}
									break;
								case 's':
								case 't':
								case 'p':
									if (color_symbols.size() > 0 || position_symbols.size() > 0) {
										mix_error = true;
										break;
									}
									if (!texture_symbols.has(c[i])) {
										texture_symbols.insert(c[i]);
									} else {
										repeated = true;
									}
									break;
								default:
									ok = false;
									break;
							}
						}

					} break;
					case TYPE_BVEC4:
					case TYPE_IVEC4:
					case TYPE_UVEC4:
					case TYPE_VEC4: {
						int l = ident.length();
						if (l == 1) {
							member_type = DataType(dt - 3);
						} else if (l == 2) {
							member_type = DataType(dt - 2);
						} else if (l == 3) {
							member_type = DataType(dt - 1);
						} else if (l == 4) {
							member_type = dt;
						} else {
							ok = false;
							break;
						}

						const char32_t *c = ident.ptr();
						for (int i = 0; i < l; i++) {
							switch (c[i]) {
								case 'r':
								case 'g':
								case 'b':
								case 'a':
									if (position_symbols.size() > 0 || texture_symbols.size() > 0) {
										mix_error = true;
										break;
									}
									if (!color_symbols.has(c[i])) {
										color_symbols.insert(c[i]);
									} else {
										repeated = true;
									}
									break;
								case 'x':
								case 'y':
								case 'z':
								case 'w':
									if (color_symbols.size() > 0 || texture_symbols.size() > 0) {
										mix_error = true;
										break;
									}
									if (!position_symbols.has(c[i])) {
										position_symbols.insert(c[i]);
									} else {
										repeated = true;
									}
									break;
								case 's':
								case 't':
								case 'p':
								case 'q':
									if (color_symbols.size() > 0 || position_symbols.size() > 0) {
										mix_error = true;
										break;
									}
									if (!texture_symbols.has(c[i])) {
										texture_symbols.insert(c[i]);
									} else {
										repeated = true;
									}
									break;
								default:
									ok = false;
									break;
							}
						}

					} break;

					default: {
						ok = false;
					}
				}

				if (mix_error) {
					_set_error(vformat(RTR("Cannot combine symbols from different sets in expression '.%s'."), ident));
					return nullptr;
				}

				if (!ok) {
					_set_error(vformat(RTR("Invalid member for '%s' expression: '.%s'."), (dt == TYPE_STRUCT ? st : get_datatype_name(dt)), ident));
					return nullptr;
				}

				MemberNode *mn = alloc_node<MemberNode>();
				mn->basetype = dt;
				mn->basetype_const = is_const;
				mn->datatype = member_type;
				mn->base_struct_name = st;
				mn->struct_name = member_struct_name;
				mn->array_size = array_size;
				mn->name = ident;
				mn->owner = expr;
				mn->has_swizzling_duplicates = repeated;

				if (array_size > 0) {
					TkPos prev_pos = _get_tkpos();
					tk = _get_token();
					if (tk.type == TK_OP_ASSIGN) {
						if (last_type == IDENTIFIER_CONSTANT) {
							_set_error(RTR("Constants cannot be modified."));
							return nullptr;
						}
						Node *assign_expression = _parse_array_constructor(p_block, p_function_info, member_type, member_struct_name, array_size);
						if (!assign_expression) {
							return nullptr;
						}
						mn->assign_expression = assign_expression;
					} else if (tk.type == TK_PERIOD) {
						completion_class = TAG_ARRAY;
						if (p_block != nullptr) {
							p_block->block_tag = SubClassTag::TAG_ARRAY;
						}
						mn->call_expression = _parse_and_reduce_expression(p_block, p_function_info);
						if (p_block != nullptr) {
							p_block->block_tag = SubClassTag::TAG_GLOBAL;
						}
						if (!mn->call_expression) {
							return nullptr;
						}
					} else if (tk.type == TK_BRACKET_OPEN) {
						Node *index_expression = _parse_and_reduce_expression(p_block, p_function_info);
						if (!index_expression) {
							return nullptr;
						}

						if (index_expression->get_array_size() != 0 || (index_expression->get_datatype() != TYPE_INT && index_expression->get_datatype() != TYPE_UINT)) {
							_set_error(RTR("Only integer expressions are allowed for indexing."));
							return nullptr;
						}

						if (index_expression->type == Node::NODE_TYPE_CONSTANT) {
							ConstantNode *cnode = static_cast<ConstantNode *>(index_expression);
							if (cnode) {
								if (!cnode->values.is_empty()) {
									int value = cnode->values[0].sint;
									if (value < 0 || value >= array_size) {
										_set_error(vformat(RTR("Index [%d] out of range [%d..%d]."), value, 0, array_size - 1));
										return nullptr;
									}
								}
							}
						}

						tk = _get_token();
						if (tk.type != TK_BRACKET_CLOSE) {
							_set_expected_error("]");
							return nullptr;
						}
						mn->index_expression = index_expression;
					} else {
						_set_tkpos(prev_pos);
					}
				}
				expr = mn;

#ifdef DEBUG_ENABLED
				keyword_completion_context = prev_keyword_completion_context;
#endif

				//todo
				//member (period) has priority over any operator
				//creates a subindexing expression in place

				/*} else if (tk.type==TK_BRACKET_OPEN) {
				//todo
				//subindexing has priority over any operator
				//creates a subindexing expression in place

	*/
			} else if (tk.type == TK_BRACKET_OPEN) {
				Node *index = _parse_and_reduce_expression(p_block, p_function_info);
				if (!index) {
					return nullptr;
				}

				if (index->get_array_size() != 0 || (index->get_datatype() != TYPE_INT && index->get_datatype() != TYPE_UINT)) {
					_set_error(RTR("Only integer expressions are allowed for indexing."));
					return nullptr;
				}

				DataType member_type = TYPE_VOID;
				String member_struct_name;

				if (expr->get_array_size() > 0) {
					if (index->type == Node::NODE_TYPE_CONSTANT) {
						uint32_t index_constant = static_cast<ConstantNode *>(index)->values[0].uint;
						if (index_constant >= (uint32_t)expr->get_array_size()) {
							_set_error(vformat(RTR("Index [%d] out of range [%d..%d]."), index_constant, 0, expr->get_array_size() - 1));
							return nullptr;
						}
					}
					member_type = expr->get_datatype();
					if (member_type == TYPE_STRUCT) {
						member_struct_name = expr->get_datatype_name();
					}
				} else {
					switch (expr->get_datatype()) {
						case TYPE_BVEC2:
						case TYPE_VEC2:
						case TYPE_IVEC2:
						case TYPE_UVEC2:
						case TYPE_MAT2:
							if (index->type == Node::NODE_TYPE_CONSTANT) {
								uint32_t index_constant = static_cast<ConstantNode *>(index)->values[0].uint;
								if (index_constant >= 2) {
									_set_error(vformat(RTR("Index [%d] out of range [%d..%d]."), index_constant, 0, 1));
									return nullptr;
								}
							}

							switch (expr->get_datatype()) {
								case TYPE_BVEC2:
									member_type = TYPE_BOOL;
									break;
								case TYPE_VEC2:
									member_type = TYPE_FLOAT;
									break;
								case TYPE_IVEC2:
									member_type = TYPE_INT;
									break;
								case TYPE_UVEC2:
									member_type = TYPE_UINT;
									break;
								case TYPE_MAT2:
									member_type = TYPE_VEC2;
									break;
								default:
									break;
							}

							break;
						case TYPE_BVEC3:
						case TYPE_VEC3:
						case TYPE_IVEC3:
						case TYPE_UVEC3:
						case TYPE_MAT3:
							if (index->type == Node::NODE_TYPE_CONSTANT) {
								uint32_t index_constant = static_cast<ConstantNode *>(index)->values[0].uint;
								if (index_constant >= 3) {
									_set_error(vformat(RTR("Index [%d] out of range [%d..%d]."), index_constant, 0, 2));
									return nullptr;
								}
							}

							switch (expr->get_datatype()) {
								case TYPE_BVEC3:
									member_type = TYPE_BOOL;
									break;
								case TYPE_VEC3:
									member_type = TYPE_FLOAT;
									break;
								case TYPE_IVEC3:
									member_type = TYPE_INT;
									break;
								case TYPE_UVEC3:
									member_type = TYPE_UINT;
									break;
								case TYPE_MAT3:
									member_type = TYPE_VEC3;
									break;
								default:
									break;
							}
							break;
						case TYPE_BVEC4:
						case TYPE_VEC4:
						case TYPE_IVEC4:
						case TYPE_UVEC4:
						case TYPE_MAT4:
							if (index->type == Node::NODE_TYPE_CONSTANT) {
								uint32_t index_constant = static_cast<ConstantNode *>(index)->values[0].uint;
								if (index_constant >= 4) {
									_set_error(vformat(RTR("Index [%d] out of range [%d..%d]."), index_constant, 0, 3));
									return nullptr;
								}
							}

							switch (expr->get_datatype()) {
								case TYPE_BVEC4:
									member_type = TYPE_BOOL;
									break;
								case TYPE_VEC4:
									member_type = TYPE_FLOAT;
									break;
								case TYPE_IVEC4:
									member_type = TYPE_INT;
									break;
								case TYPE_UVEC4:
									member_type = TYPE_UINT;
									break;
								case TYPE_MAT4:
									member_type = TYPE_VEC4;
									break;
								default:
									break;
							}
							break;
						default: {
							_set_error(vformat(RTR("An object of type '%s' can't be indexed."), (expr->get_datatype() == TYPE_STRUCT ? expr->get_datatype_name() : get_datatype_name(expr->get_datatype()))));
							return nullptr;
						}
					}
				}
				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = OP_INDEX;
				op->return_cache = member_type;
				op->struct_name = member_struct_name;
				op->arguments.push_back(expr);
				op->arguments.push_back(index);
				expr = op;

				tk = _get_token();
				if (tk.type != TK_BRACKET_CLOSE) {
					_set_expected_error("]");
					return nullptr;
				}

			} else if (tk.type == TK_OP_INCREMENT || tk.type == TK_OP_DECREMENT) {
				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = tk.type == TK_OP_DECREMENT ? OP_POST_DECREMENT : OP_POST_INCREMENT;
				op->arguments.push_back(expr);

				if (!_validate_operator(p_block, op, &op->return_cache, &op->return_array_size)) {
					_set_error(RTR("Invalid base type for increment/decrement operator."));
					return nullptr;
				}

				if (!_validate_assign(expr, p_function_info)) {
					_set_error(RTR("Invalid use of increment/decrement operator in a constant expression."));
					return nullptr;
				}
				expr = op;
			} else {
				_set_tkpos(pos2);
				break;
			}
		}

		Expression e;
		e.is_op = false;
		e.node = expr;
		expression.push_back(e);

		pos = _get_tkpos();
		tk = _get_token();

		if (p_previous_expression_info != nullptr && tk.type == p_previous_expression_info->tt_break && !p_previous_expression_info->is_last_expr) {
			break;
		}

		if (is_token_operator(tk.type)) {
			Expression o;
			o.is_op = true;

			switch (tk.type) {
				case TK_OP_EQUAL:
					o.op = OP_EQUAL;
					break;
				case TK_OP_NOT_EQUAL:
					o.op = OP_NOT_EQUAL;
					break;
				case TK_OP_LESS:
					o.op = OP_LESS;
					break;
				case TK_OP_LESS_EQUAL:
					o.op = OP_LESS_EQUAL;
					break;
				case TK_OP_GREATER:
					o.op = OP_GREATER;
					break;
				case TK_OP_GREATER_EQUAL:
					o.op = OP_GREATER_EQUAL;
					break;
				case TK_OP_AND:
					o.op = OP_AND;
					break;
				case TK_OP_OR:
					o.op = OP_OR;
					break;
				case TK_OP_ADD:
					o.op = OP_ADD;
					break;
				case TK_OP_SUB:
					o.op = OP_SUB;
					break;
				case TK_OP_MUL:
					o.op = OP_MUL;
					break;
				case TK_OP_DIV:
					o.op = OP_DIV;
					break;
				case TK_OP_MOD:
					o.op = OP_MOD;
					break;
				case TK_OP_SHIFT_LEFT:
					o.op = OP_SHIFT_LEFT;
					break;
				case TK_OP_SHIFT_RIGHT:
					o.op = OP_SHIFT_RIGHT;
					break;
				case TK_OP_ASSIGN:
					o.op = OP_ASSIGN;
					break;
				case TK_OP_ASSIGN_ADD:
					o.op = OP_ASSIGN_ADD;
					break;
				case TK_OP_ASSIGN_SUB:
					o.op = OP_ASSIGN_SUB;
					break;
				case TK_OP_ASSIGN_MUL:
					o.op = OP_ASSIGN_MUL;
					break;
				case TK_OP_ASSIGN_DIV:
					o.op = OP_ASSIGN_DIV;
					break;
				case TK_OP_ASSIGN_MOD:
					o.op = OP_ASSIGN_MOD;
					break;
				case TK_OP_ASSIGN_SHIFT_LEFT:
					o.op = OP_ASSIGN_SHIFT_LEFT;
					break;
				case TK_OP_ASSIGN_SHIFT_RIGHT:
					o.op = OP_ASSIGN_SHIFT_RIGHT;
					break;
				case TK_OP_ASSIGN_BIT_AND:
					o.op = OP_ASSIGN_BIT_AND;
					break;
				case TK_OP_ASSIGN_BIT_OR:
					o.op = OP_ASSIGN_BIT_OR;
					break;
				case TK_OP_ASSIGN_BIT_XOR:
					o.op = OP_ASSIGN_BIT_XOR;
					break;
				case TK_OP_BIT_AND:
					o.op = OP_BIT_AND;
					break;
				case TK_OP_BIT_OR:
					o.op = OP_BIT_OR;
					break;
				case TK_OP_BIT_XOR:
					o.op = OP_BIT_XOR;
					break;
				case TK_QUESTION:
					o.op = OP_SELECT_IF;
					break;
				case TK_COLON:
					o.op = OP_SELECT_ELSE;
					break;
				default: {
					_set_error(vformat(RTR("Invalid token for the operator: '%s'."), get_token_text(tk)));
					return nullptr;
				}
			}

			expression.push_back(o);

			if (o.op == OP_SELECT_IF) {
				ExpressionInfo info;
				info.expression = &expression;
				info.tt_break = TK_COLON;

				expr = _parse_and_reduce_expression(p_block, p_function_info, &info);
				if (!expr) {
					return nullptr;
				}

				expression.push_back({ true, { OP_SELECT_ELSE } });

				if (p_previous_expression_info != nullptr) {
					info.is_last_expr = p_previous_expression_info->is_last_expr;
				} else {
					info.is_last_expr = true;
				}

				expr = _parse_and_reduce_expression(p_block, p_function_info, &info);
				if (!expr) {
					return nullptr;
				}

				break;
			}
		} else {
			_set_tkpos(pos); //something else, so rollback and end
			break;
		}
	}

	/* Reduce the set set of expressions and place them in an operator tree, respecting precedence */

	while (expression.size() > 1) {
		int next_op = -1;
		int min_priority = 0xFFFFF;
		bool is_unary = false;
		bool is_ternary = false;

		for (int i = 0; i < expression.size(); i++) {
			if (!expression[i].is_op) {
				continue;
			}

			bool unary = false;
			bool ternary = false;
			Operator op = expression[i].op;

			int priority;
			switch (op) {
				case OP_EQUAL:
					priority = 8;
					break;
				case OP_NOT_EQUAL:
					priority = 8;
					break;
				case OP_LESS:
					priority = 7;
					break;
				case OP_LESS_EQUAL:
					priority = 7;
					break;
				case OP_GREATER:
					priority = 7;
					break;
				case OP_GREATER_EQUAL:
					priority = 7;
					break;
				case OP_AND:
					priority = 12;
					break;
				case OP_OR:
					priority = 14;
					break;
				case OP_NOT:
					priority = 3;
					unary = true;
					break;
				case OP_NEGATE:
					priority = 3;
					unary = true;
					break;
				case OP_ADD:
					priority = 5;
					break;
				case OP_SUB:
					priority = 5;
					break;
				case OP_MUL:
					priority = 4;
					break;
				case OP_DIV:
					priority = 4;
					break;
				case OP_MOD:
					priority = 4;
					break;
				case OP_SHIFT_LEFT:
					priority = 6;
					break;
				case OP_SHIFT_RIGHT:
					priority = 6;
					break;
				case OP_ASSIGN:
					priority = 16;
					break;
				case OP_ASSIGN_ADD:
					priority = 16;
					break;
				case OP_ASSIGN_SUB:
					priority = 16;
					break;
				case OP_ASSIGN_MUL:
					priority = 16;
					break;
				case OP_ASSIGN_DIV:
					priority = 16;
					break;
				case OP_ASSIGN_MOD:
					priority = 16;
					break;
				case OP_ASSIGN_SHIFT_LEFT:
					priority = 16;
					break;
				case OP_ASSIGN_SHIFT_RIGHT:
					priority = 16;
					break;
				case OP_ASSIGN_BIT_AND:
					priority = 16;
					break;
				case OP_ASSIGN_BIT_OR:
					priority = 16;
					break;
				case OP_ASSIGN_BIT_XOR:
					priority = 16;
					break;
				case OP_BIT_AND:
					priority = 9;
					break;
				case OP_BIT_OR:
					priority = 11;
					break;
				case OP_BIT_XOR:
					priority = 10;
					break;
				case OP_BIT_INVERT:
					priority = 3;
					unary = true;
					break;
				case OP_INCREMENT:
					priority = 3;
					unary = true;
					break;
				case OP_DECREMENT:
					priority = 3;
					unary = true;
					break;
				case OP_SELECT_IF:
					priority = 15;
					ternary = true;
					break;
				case OP_SELECT_ELSE:
					priority = 15;
					ternary = true;
					break;

				default:
					ERR_FAIL_V(nullptr); //unexpected operator
			}

#ifdef DEBUG_ENABLED
			if (check_warnings && HAS_WARNING(ShaderWarning::FLOAT_COMPARISON_FLAG) && (op == OP_EQUAL || op == OP_NOT_EQUAL) &&
					(!expression[i - 1].is_op && !expression[i + 1].is_op) &&
					(expression[i - 1].node->get_datatype() == TYPE_FLOAT && expression[i + 1].node->get_datatype() == TYPE_FLOAT)) {
				_add_line_warning(ShaderWarning::FLOAT_COMPARISON);
			}
#endif // DEBUG_ENABLED

			if (priority < min_priority) {
				// < is used for left to right (default)
				// <= is used for right to left
				next_op = i;
				min_priority = priority;
				is_unary = unary;
				is_ternary = ternary;
			}
		}

		ERR_FAIL_COND_V(next_op == -1, nullptr);

		// OK! create operator..
		if (is_unary) {
			int expr_pos = next_op;
			while (expression[expr_pos].is_op) {
				expr_pos++;
				if (expr_pos == expression.size()) {
					//can happen..
					_set_error(RTR("Unexpected end of expression."));
					return nullptr;
				}
			}

			//consecutively do unary operators
			for (int i = expr_pos - 1; i >= next_op; i--) {
				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = expression[i].op;
				if ((op->op == OP_INCREMENT || op->op == OP_DECREMENT) && !_validate_assign(expression[i + 1].node, p_function_info)) {
					_set_error(RTR("Invalid use of increment/decrement operator in a constant expression."));
					return nullptr;
				}
				op->arguments.push_back(expression[i + 1].node);

				expression.write[i].is_op = false;
				expression.write[i].node = op;

				if (!_validate_operator(p_block, op, &op->return_cache, &op->return_array_size)) {
					if (error_set) {
						return nullptr;
					}

					String at;
					for (int j = 0; j < op->arguments.size(); j++) {
						if (j > 0) {
							at += ", ";
						}
						at += get_datatype_name(op->arguments[j]->get_datatype());
						if (!op->arguments[j]->is_indexed() && op->arguments[j]->get_array_size() > 0) {
							at += "[";
							at += itos(op->arguments[j]->get_array_size());
							at += "]";
						}
					}
					_set_error(vformat(RTR("Invalid arguments to unary operator '%s': %s."), get_operator_text(op->op), at));
					return nullptr;
				}
				expression.remove_at(i + 1);
			}

		} else if (is_ternary) {
			if (next_op < 1 || next_op >= (expression.size() - 1)) {
				_set_parsing_error();
				ERR_FAIL_V(nullptr);
			}

			if (next_op + 2 >= expression.size() || !expression[next_op + 2].is_op || expression[next_op + 2].op != OP_SELECT_ELSE) {
				_set_error(RTR("Missing matching ':' for select operator."));
				return nullptr;
			}

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = expression[next_op].op;
			op->arguments.push_back(expression[next_op - 1].node);
			op->arguments.push_back(expression[next_op + 1].node);
			op->arguments.push_back(expression[next_op + 3].node);

			expression.write[next_op - 1].is_op = false;
			expression.write[next_op - 1].node = op;
			if (!_validate_operator(p_block, op, &op->return_cache, &op->return_array_size)) {
				if (error_set) {
					return nullptr;
				}

				String at;
				for (int i = 0; i < op->arguments.size(); i++) {
					if (i > 0) {
						at += ", ";
					}
					at += get_datatype_name(op->arguments[i]->get_datatype());
					if (!op->arguments[i]->is_indexed() && op->arguments[i]->get_array_size() > 0) {
						at += "[";
						at += itos(op->arguments[i]->get_array_size());
						at += "]";
					}
				}
				_set_error(vformat(RTR("Invalid argument to ternary operator: '%s'."), at));
				return nullptr;
			}

			for (int i = 0; i < 4; i++) {
				expression.remove_at(next_op);
			}

		} else {
			if (next_op < 1 || next_op >= (expression.size() - 1)) {
				_set_parsing_error();
				ERR_FAIL_V(nullptr);
			}

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = expression[next_op].op;

			if (expression[next_op - 1].is_op) {
				_set_parsing_error();
				ERR_FAIL_V(nullptr);
			}

			if (_is_operator_assign(op->op)) {
				if (p_block && expression[next_op - 1].node->type == Node::NODE_TYPE_VARIABLE) {
					VariableNode *vn = static_cast<VariableNode *>(expression[next_op - 1].node);
					p_block->use_op_eval = vn->is_const;
				}

				String assign_message;
				if (!_validate_assign(expression[next_op - 1].node, p_function_info, &assign_message)) {
					_set_error(assign_message);
					return nullptr;
				}
			}

			if (expression[next_op + 1].is_op) {
				// this is not invalid and can really appear
				// but it becomes invalid anyway because no binary op
				// can be followed by a unary op in a valid combination,
				// due to how precedence works, unaries will always disappear first

				_set_parsing_error();
			}

			op->arguments.push_back(expression[next_op - 1].node); //expression goes as left
			op->arguments.push_back(expression[next_op + 1].node); //next expression goes as right
			expression.write[next_op - 1].node = op;

			//replace all 3 nodes by this operator and make it an expression

			if (!_validate_operator(p_block, op, &op->return_cache, &op->return_array_size)) {
				if (error_set) {
					return nullptr;
				}

				String at;
				for (int i = 0; i < op->arguments.size(); i++) {
					if (i > 0) {
						at += ", ";
					}
					if (op->arguments[i]->get_datatype() == TYPE_STRUCT) {
						at += op->arguments[i]->get_datatype_name();
					} else {
						at += get_datatype_name(op->arguments[i]->get_datatype());
					}
					if (!op->arguments[i]->is_indexed() && op->arguments[i]->get_array_size() > 0) {
						at += "[";
						at += itos(op->arguments[i]->get_array_size());
						at += "]";
					}
				}
				_set_error(vformat(RTR("Invalid arguments to operator '%s': '%s'."), get_operator_text(op->op), at));
				return nullptr;
			}

			expression.remove_at(next_op);
			expression.remove_at(next_op);
		}
	}

	if (p_block) {
		p_block->use_op_eval = true;
	}

	if (p_previous_expression_info != nullptr) {
		p_previous_expression_info->expression->push_back(expression[0]);
	}

	return expression[0].node;
}

ShaderLanguage::Node *ShaderLanguage::_reduce_expression(BlockNode *p_block, ShaderLanguage::Node *p_node) {
	if (p_node->type != Node::NODE_TYPE_OPERATOR) {
		return p_node;
	}

	//for now only reduce simple constructors
	OperatorNode *op = static_cast<OperatorNode *>(p_node);

	if (op->op == OP_CONSTRUCT) {
		ERR_FAIL_COND_V(op->arguments[0]->type != Node::NODE_TYPE_VARIABLE, p_node);

		DataType type = op->get_datatype();
		DataType base = get_scalar_type(type);
		int cardinality = get_cardinality(type);

		Vector<Scalar> values;

		for (int i = 1; i < op->arguments.size(); i++) {
			op->arguments.write[i] = _reduce_expression(p_block, op->arguments[i]);
			if (op->arguments[i]->type == Node::NODE_TYPE_CONSTANT) {
				ConstantNode *cn = static_cast<ConstantNode *>(op->arguments[i]);

				if (get_scalar_type(cn->datatype) == base) {
					for (int j = 0; j < cn->values.size(); j++) {
						values.push_back(cn->values[j]);
					}
				} else if (get_scalar_type(cn->datatype) == cn->datatype) {
					Scalar v;
					if (!convert_constant(cn, base, &v)) {
						return p_node;
					}
					values.push_back(v);
				} else {
					return p_node;
				}

			} else {
				return p_node;
			}
		}

		if (values.size() == 1) {
			if (type >= TYPE_MAT2 && type <= TYPE_MAT4) {
				Scalar value = values[0];
				Scalar zero;
				zero.real = 0.0f;
				int size = 2 + (type - TYPE_MAT2);

				values.clear();
				for (int i = 0; i < size; i++) {
					for (int j = 0; j < size; j++) {
						values.push_back(i == j ? value : zero);
					}
				}
			} else {
				Scalar value = values[0];
				for (int i = 1; i < cardinality; i++) {
					values.push_back(value);
				}
			}
		} else if (values.size() != cardinality) {
			ERR_PRINT("Failed to reduce expression, values and cardinality mismatch.");
			return p_node;
		}

		ConstantNode *cn = alloc_node<ConstantNode>();
		cn->datatype = op->get_datatype();
		cn->values = values;
		return cn;
	} else if (op->op == OP_NEGATE) {
		op->arguments.write[0] = _reduce_expression(p_block, op->arguments[0]);
		if (op->arguments[0]->type == Node::NODE_TYPE_CONSTANT) {
			ConstantNode *cn = static_cast<ConstantNode *>(op->arguments[0]);

			DataType base = get_scalar_type(cn->datatype);

			Vector<Scalar> values;

			for (int i = 0; i < cn->values.size(); i++) {
				Scalar nv;
				switch (base) {
					case TYPE_BOOL: {
						nv.boolean = !cn->values[i].boolean;
					} break;
					case TYPE_INT: {
						nv.sint = -cn->values[i].sint;
					} break;
					case TYPE_UINT: {
						// Intentionally wrap the unsigned int value, because GLSL does.
						nv.uint = 0 - cn->values[i].uint;
					} break;
					case TYPE_FLOAT: {
						nv.real = -cn->values[i].real;
					} break;
					default: {
					}
				}

				values.push_back(nv);
			}

			cn->values = values;
			return cn;
		}
	}

	return p_node;
}

ShaderLanguage::Node *ShaderLanguage::_parse_and_reduce_expression(BlockNode *p_block, const FunctionInfo &p_function_info, const ExpressionInfo *p_previous_expression_info) {
	ShaderLanguage::Node *expr = _parse_expression(p_block, p_function_info, p_previous_expression_info);
	if (!expr) { //errored
		return nullptr;
	}

	expr = _reduce_expression(p_block, expr);

	return expr;
}

Error ShaderLanguage::_parse_block(BlockNode *p_block, const FunctionInfo &p_function_info, bool p_just_one, bool p_can_break, bool p_can_continue) {
	while (true) {
		TkPos pos = _get_tkpos();
		Token tk = _get_token();
#ifdef DEBUG_ENABLED
		Token next;
#endif // DEBUG_ENABLED

		if (p_block && p_block->block_type == BlockNode::BLOCK_TYPE_SWITCH) {
			if (tk.type != TK_CF_CASE && tk.type != TK_CF_DEFAULT && tk.type != TK_CURLY_BRACKET_CLOSE) {
				_set_error(vformat(RTR("A switch may only contain '%s' and '%s' blocks."), "case", "default"));
				return ERR_PARSE_ERROR;
			}
		}

		bool is_struct = shader->structs.has(tk.text);
		bool is_var_init = false;
		bool is_condition = false;

		if (tk.type == TK_CURLY_BRACKET_CLOSE) { //end of block
			if (p_just_one) {
				_set_expected_error("}");
				return ERR_PARSE_ERROR;
			}

			return OK;

		} else if (tk.type == TK_CONST || is_token_precision(tk.type) || is_token_nonvoid_datatype(tk.type) || is_struct) {
			is_var_init = true;

			String struct_name = "";
			if (is_struct) {
				struct_name = tk.text;
#ifdef DEBUG_ENABLED
				if (check_warnings && HAS_WARNING(ShaderWarning::UNUSED_STRUCT_FLAG) && used_structs.has(struct_name)) {
					used_structs[struct_name].used = true;
				}
#endif // DEBUG_ENABLED
			}
#ifdef DEBUG_ENABLED
			uint32_t precision_flag = CF_PRECISION_MODIFIER;

			keyword_completion_context = CF_DATATYPE;
			if (!is_token_precision(tk.type)) {
				if (!is_struct) {
					keyword_completion_context |= precision_flag;
				}
			}
#endif // DEBUG_ENABLED

			bool is_const = false;

			if (tk.type == TK_CONST) {
				is_const = true;
				tk = _get_token();

				if (!is_struct) {
					is_struct = shader->structs.has(tk.text); // check again.
					struct_name = tk.text;
				}
			}

			DataPrecision precision = PRECISION_DEFAULT;
			if (is_token_precision(tk.type)) {
				precision = get_token_precision(tk.type);
				tk = _get_token();

				if (!is_struct) {
					is_struct = shader->structs.has(tk.text); // check again.
				}

#ifdef DEBUG_ENABLED
				if (keyword_completion_context & precision_flag) {
					keyword_completion_context ^= precision_flag;
				}
#endif // DEBUG_ENABLED
			}

#ifdef DEBUG_ENABLED
			if (is_const && _lookup_next(next)) {
				if (is_token_precision(next.type)) {
					keyword_completion_context = CF_UNSPECIFIED;
				}
				if (is_token_datatype(next.type)) {
					keyword_completion_context ^= CF_DATATYPE;
				}
			}
#endif // DEBUG_ENABLED

			if (precision != PRECISION_DEFAULT) {
				if (!is_token_nonvoid_datatype(tk.type)) {
					_set_error(RTR("Expected variable type after precision modifier."));
					return ERR_PARSE_ERROR;
				}
			}

			if (!is_struct) {
				if (!is_token_variable_datatype(tk.type)) {
					_set_error(RTR("Invalid variable type (samplers are not allowed)."));
					return ERR_PARSE_ERROR;
				}
			}

			DataType type = is_struct ? TYPE_STRUCT : get_token_datatype(tk.type);

			if (precision != PRECISION_DEFAULT && _validate_precision(type, precision) != OK) {
				return ERR_PARSE_ERROR;
			}

#ifdef DEBUG_ENABLED
			keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED

			int array_size = 0;
			bool fixed_array_size = false;
			bool first = true;

			VariableDeclarationNode *vdnode = alloc_node<VariableDeclarationNode>();
			vdnode->precision = precision;
			if (is_struct) {
				vdnode->struct_name = struct_name;
				vdnode->datatype = TYPE_STRUCT;
			} else {
				vdnode->datatype = type;
			};
			vdnode->is_const = is_const;

			do {
				bool unknown_size = false;
				VariableDeclarationNode::Declaration decl;

				tk = _get_token();

				if (first) {
					first = false;

					if (tk.type != TK_IDENTIFIER && tk.type != TK_BRACKET_OPEN) {
						_set_error(RTR("Expected an identifier or '[' after type."));
						return ERR_PARSE_ERROR;
					}

					if (tk.type == TK_BRACKET_OPEN) {
						Error error = _parse_array_size(p_block, p_function_info, false, &decl.size_expression, &array_size, &unknown_size);
						if (error != OK) {
							return error;
						}
						decl.size = array_size;

						fixed_array_size = true;
						tk = _get_token();
					}
				}

				if (tk.type != TK_IDENTIFIER) {
					_set_error(RTR("Expected an identifier."));
					return ERR_PARSE_ERROR;
				}

				StringName name = tk.text;
				ShaderLanguage::IdentifierType itype;
				if (_find_identifier(p_block, true, p_function_info, name, (ShaderLanguage::DataType *)nullptr, &itype)) {
					if (itype != IDENTIFIER_FUNCTION) {
						_set_redefinition_error(String(name));
						return ERR_PARSE_ERROR;
					}
				}
				decl.name = name;

#ifdef DEBUG_ENABLED
				if (check_warnings && HAS_WARNING(ShaderWarning::UNUSED_LOCAL_VARIABLE_FLAG) && p_block) {
					FunctionNode *parent_function = nullptr;
					{
						BlockNode *block = p_block;
						while (block && !block->parent_function) {
							block = block->parent_block;
						}
						parent_function = block->parent_function;
					}
					if (parent_function) {
						StringName func_name = parent_function->name;

						if (!used_local_vars.has(func_name)) {
							used_local_vars.insert(func_name, HashMap<StringName, Usage>());
						}

						used_local_vars[func_name].insert(name, Usage(tk_line));
					}
				}
#endif // DEBUG_ENABLED
				is_const_decl = is_const;

				BlockNode::Variable var;
				var.type = type;
				var.precision = precision;
				var.line = tk_line;
				var.array_size = array_size;
				var.is_const = is_const;
				var.struct_name = struct_name;

				tk = _get_token();

				if (tk.type == TK_BRACKET_OPEN) {
					Error error = _parse_array_size(p_block, p_function_info, false, &decl.size_expression, &var.array_size, &unknown_size);
					if (error != OK) {
						return error;
					}

					decl.size = var.array_size;
					array_size = var.array_size;

					tk = _get_token();
				}
#ifdef DEBUG_ENABLED
				if (var.type == DataType::TYPE_BOOL) {
					keyword_completion_context = CF_BOOLEAN;
				}
#endif // DEBUG_ENABLED
				if (var.array_size > 0 || unknown_size) {
					bool full_def = false;

					if (tk.type == TK_OP_ASSIGN) {
						TkPos prev_pos = _get_tkpos();
						tk = _get_token();

						if (tk.type == TK_IDENTIFIER) { // a function call array initialization
							_set_tkpos(prev_pos);
							Node *n = _parse_and_reduce_expression(p_block, p_function_info);

							if (!n) {
								_set_error(RTR("Expected array initializer."));
								return ERR_PARSE_ERROR;
							} else {
								if (unknown_size) {
									decl.size = n->get_array_size();
									var.array_size = n->get_array_size();
								}

								if (!_compare_datatypes(var.type, var.struct_name, var.array_size, n->get_datatype(), n->get_datatype_name(), n->get_array_size())) {
									return ERR_PARSE_ERROR;
								}

								decl.single_expression = true;
								decl.initializer.push_back(n);
							}

							tk = _get_token();
						} else {
							if (tk.type != TK_CURLY_BRACKET_OPEN) {
								if (unknown_size) {
									_set_expected_error("{");
									return ERR_PARSE_ERROR;
								}

								full_def = true;

								DataPrecision precision2 = PRECISION_DEFAULT;
								if (is_token_precision(tk.type)) {
									precision2 = get_token_precision(tk.type);
									tk = _get_token();
									if (!is_token_nonvoid_datatype(tk.type)) {
										_set_error(RTR("Expected data type after precision modifier."));
										return ERR_PARSE_ERROR;
									}
								}

								DataType type2;
								StringName struct_name2 = "";

								if (shader->structs.has(tk.text)) {
									type2 = TYPE_STRUCT;
									struct_name2 = tk.text;
								} else {
									if (!is_token_variable_datatype(tk.type)) {
										_set_error(RTR("Invalid data type for the array."));
										return ERR_PARSE_ERROR;
									}
									type2 = get_token_datatype(tk.type);
								}

								if (precision2 != PRECISION_DEFAULT && _validate_precision(type2, precision2) != OK) {
									return ERR_PARSE_ERROR;
								}

								int array_size2 = 0;

								tk = _get_token();
								if (tk.type == TK_BRACKET_OPEN) {
									bool is_unknown_size = false;
									Error error = _parse_array_size(p_block, p_function_info, false, nullptr, &array_size2, &is_unknown_size);
									if (error != OK) {
										return error;
									}
									if (is_unknown_size) {
										array_size2 = var.array_size;
									}
									tk = _get_token();
								} else {
									_set_expected_error("[");
									return ERR_PARSE_ERROR;
								}

								if (precision != precision2 || type != type2 || struct_name != struct_name2 || var.array_size != array_size2) {
									String from;
									if (precision2 != PRECISION_DEFAULT) {
										from += get_precision_name(precision2);
										from += " ";
									}
									if (type2 == TYPE_STRUCT) {
										from += struct_name2;
									} else {
										from += get_datatype_name(type2);
									}
									from += "[";
									from += itos(array_size2);
									from += "]'";

									String to;
									if (precision != PRECISION_DEFAULT) {
										to += get_precision_name(precision);
										to += " ";
									}
									if (type == TYPE_STRUCT) {
										to += struct_name;
									} else {
										to += get_datatype_name(type);
									}
									to += "[";
									to += itos(var.array_size);
									to += "]'";

									_set_error(vformat(RTR("Cannot convert from '%s' to '%s'."), from, to));
									return ERR_PARSE_ERROR;
								}
							}

							bool curly = tk.type == TK_CURLY_BRACKET_OPEN;

							if (unknown_size) {
								if (!curly) {
									_set_expected_error("{");
									return ERR_PARSE_ERROR;
								}
							} else {
								if (full_def) {
									if (curly) {
										_set_expected_error("(");
										return ERR_PARSE_ERROR;
									}
								}
							}

							if (tk.type == TK_PARENTHESIS_OPEN || curly) { // initialization
								while (true) {
									Node *n = _parse_and_reduce_expression(p_block, p_function_info);
									if (!n) {
										return ERR_PARSE_ERROR;
									}

									if (is_const && n->type == Node::NODE_TYPE_OPERATOR && static_cast<OperatorNode *>(n)->op == OP_CALL) {
										_set_error(RTR("Expected a constant expression."));
										return ERR_PARSE_ERROR;
									}

									if (!_compare_datatypes(var.type, struct_name, 0, n->get_datatype(), n->get_datatype_name(), 0)) {
										return ERR_PARSE_ERROR;
									}

									tk = _get_token();
									if (tk.type == TK_COMMA) {
										decl.initializer.push_back(n);
										continue;
									} else if (!curly && tk.type == TK_PARENTHESIS_CLOSE) {
										decl.initializer.push_back(n);
										break;
									} else if (curly && tk.type == TK_CURLY_BRACKET_CLOSE) {
										decl.initializer.push_back(n);
										break;
									} else {
										if (curly) {
											_set_expected_error("}", ",");
										} else {
											_set_expected_error(")", ",");
										}
										return ERR_PARSE_ERROR;
									}
								}
								if (unknown_size) {
									decl.size = decl.initializer.size();
									var.array_size = decl.initializer.size();
								} else if (decl.initializer.size() != var.array_size) {
									_set_error(vformat(RTR("Array size mismatch. Expected %d elements (found %d)."), var.array_size, decl.initializer.size()));
									return ERR_PARSE_ERROR;
								}
								tk = _get_token();
							} else {
								_set_expected_error("(");
								return ERR_PARSE_ERROR;
							}
						}
					} else {
						if (unknown_size) {
							_set_error(RTR("Expected array initialization."));
							return ERR_PARSE_ERROR;
						}
						if (is_const) {
							_set_error(RTR("Expected initialization of constant."));
							return ERR_PARSE_ERROR;
						}
					}

					array_size = var.array_size;
				} else if (tk.type == TK_OP_ASSIGN) {
					p_block->use_op_eval = is_const;

					// Variable created with assignment! Must parse an expression.
					Node *n = _parse_and_reduce_expression(p_block, p_function_info);
					if (!n) {
						return ERR_PARSE_ERROR;
					}
					if (is_const && n->type == Node::NODE_TYPE_OPERATOR && static_cast<OperatorNode *>(n)->op == OP_CALL) {
						OperatorNode *op = static_cast<OperatorNode *>(n);
						for (int i = 1; i < op->arguments.size(); i++) {
							if (!_check_node_constness(op->arguments[i])) {
								_set_error(vformat(RTR("Expected constant expression for argument %d of function call after '='."), i - 1));
								return ERR_PARSE_ERROR;
							}
						}
					}

					if (is_const) {
						var.values = n->get_values();
					}

					if (!_compare_datatypes(var.type, var.struct_name, var.array_size, n->get_datatype(), n->get_datatype_name(), n->get_array_size())) {
						return ERR_PARSE_ERROR;
					}

					decl.initializer.push_back(n);
					tk = _get_token();
				} else {
					if (is_const) {
						_set_error(RTR("Expected initialization of constant."));
						return ERR_PARSE_ERROR;
					}
				}

				vdnode->declarations.push_back(decl);
				p_block->variables[name] = var;
				is_const_decl = false;

				if (!fixed_array_size) {
					array_size = 0;
				}

				if (tk.type == TK_SEMICOLON) {
					break;
				} else if (tk.type != TK_COMMA) {
					_set_expected_error(",", ";");
					return ERR_PARSE_ERROR;
				}
			} while (tk.type == TK_COMMA); //another variable
#ifdef DEBUG_ENABLED
			keyword_completion_context = CF_BLOCK;
#endif // DEBUG_ENABLED
			p_block->statements.push_back(static_cast<Node *>(vdnode));
		} else if (tk.type == TK_CURLY_BRACKET_OPEN) {
			//a sub block, just because..
			BlockNode *block = alloc_node<BlockNode>();
			block->parent_block = p_block;
			if (_parse_block(block, p_function_info, false, p_can_break, p_can_continue) != OK) {
				return ERR_PARSE_ERROR;
			}
			p_block->statements.push_back(block);
		} else if (tk.type == TK_CF_IF) {
			//if () {}
			tk = _get_token();
			if (tk.type != TK_PARENTHESIS_OPEN) {
				_set_expected_after_error("(", "if");
				return ERR_PARSE_ERROR;
			}

			ControlFlowNode *cf = alloc_node<ControlFlowNode>();
			cf->flow_op = FLOW_OP_IF;
#ifdef DEBUG_ENABLED
			keyword_completion_context = CF_IF_DECL;
#endif // DEBUG_ENABLED
			Node *n = _parse_and_reduce_expression(p_block, p_function_info);
			if (!n) {
				return ERR_PARSE_ERROR;
			}
#ifdef DEBUG_ENABLED
			keyword_completion_context = CF_BLOCK;
#endif // DEBUG_ENABLED

			if (n->get_datatype() != TYPE_BOOL) {
				_set_error(RTR("Expected a boolean expression."));
				return ERR_PARSE_ERROR;
			}

			tk = _get_token();
			if (tk.type != TK_PARENTHESIS_CLOSE) {
				_set_expected_error(")");
				return ERR_PARSE_ERROR;
			}

			BlockNode *block = alloc_node<BlockNode>();
			block->parent_block = p_block;
			cf->expressions.push_back(n);
			cf->blocks.push_back(block);
			p_block->statements.push_back(cf);

			Error err = _parse_block(block, p_function_info, true, p_can_break, p_can_continue);
			if (err) {
				return err;
			}

			pos = _get_tkpos();
			tk = _get_token();
			if (tk.type == TK_CF_ELSE) {
				block = alloc_node<BlockNode>();
				block->parent_block = p_block;
				cf->blocks.push_back(block);
				err = _parse_block(block, p_function_info, true, p_can_break, p_can_continue);
				if (err) {
					return err;
				}
			} else {
				_set_tkpos(pos); //rollback
			}
		} else if (tk.type == TK_CF_SWITCH) {
			// switch() {}
			tk = _get_token();
			if (tk.type != TK_PARENTHESIS_OPEN) {
				_set_expected_after_error("(", "switch");
				return ERR_PARSE_ERROR;
			}
			ControlFlowNode *cf = alloc_node<ControlFlowNode>();
			cf->flow_op = FLOW_OP_SWITCH;
			Node *n = _parse_and_reduce_expression(p_block, p_function_info);
			if (!n) {
				return ERR_PARSE_ERROR;
			}
			const ShaderLanguage::DataType data_type = n->get_datatype();
			if (data_type != TYPE_INT && data_type != TYPE_UINT) {
				_set_error(RTR("Expected an integer or unsigned integer expression."));
				return ERR_PARSE_ERROR;
			}
			tk = _get_token();
			if (tk.type != TK_PARENTHESIS_CLOSE) {
				_set_expected_error(")");
				return ERR_PARSE_ERROR;
			}
			tk = _get_token();
			if (tk.type != TK_CURLY_BRACKET_OPEN) {
				_set_expected_after_error("{", "switch");
				return ERR_PARSE_ERROR;
			}
			BlockNode *switch_block = alloc_node<BlockNode>();
			switch_block->block_type = BlockNode::BLOCK_TYPE_SWITCH;
			switch_block->parent_block = p_block;
			switch_block->expected_type = data_type;
			cf->expressions.push_back(n);
			cf->blocks.push_back(switch_block);
			p_block->statements.push_back(cf);

			pos = _get_tkpos();
			tk = _get_token();
			TokenType prev_type;
			if (tk.type == TK_CF_CASE || tk.type == TK_CF_DEFAULT) {
				prev_type = tk.type;
				_set_tkpos(pos);
			} else {
				_set_expected_error("case", "default");
				return ERR_PARSE_ERROR;
			}

			while (true) { // Go-through multiple cases.

				if (_parse_block(switch_block, p_function_info, true, true, false) != OK) {
					return ERR_PARSE_ERROR;
				}
				pos = _get_tkpos();
				tk = _get_token();
				if (tk.type == TK_CF_CASE || tk.type == TK_CF_DEFAULT) {
					if (prev_type == TK_CF_DEFAULT) {
						if (tk.type == TK_CF_CASE) {
							_set_error(RTR("Cases must be defined before default case."));
							return ERR_PARSE_ERROR;
						} else if (prev_type == TK_CF_DEFAULT) {
							_set_error(RTR("Default case must be defined only once."));
							return ERR_PARSE_ERROR;
						}
					}
					prev_type = tk.type;
					_set_tkpos(pos);
					continue;
				} else {
					break;
				}
			}

		} else if (tk.type == TK_CF_CASE) {
			// case x : break; | return;

			if (p_block && p_block->block_type == BlockNode::BLOCK_TYPE_CASE) {
				_set_tkpos(pos);
				return OK;
			}

			if (!p_block || (p_block->block_type != BlockNode::BLOCK_TYPE_SWITCH)) {
				_set_error(vformat(RTR("'%s' must be placed within a '%s' block."), "case", "switch"));
				return ERR_PARSE_ERROR;
			}

			tk = _get_token();

			int sign = 1;

			if (tk.type == TK_OP_SUB) {
				sign = -1;
				tk = _get_token();
			}

			Node *n = nullptr;

			if (!tk.is_integer_constant()) {
				bool correct_constant_expression = false;

				if (tk.type == TK_IDENTIFIER) {
					DataType data_type;
					bool is_const;

					_find_identifier(p_block, false, p_function_info, tk.text, &data_type, nullptr, &is_const);
					if (is_const && data_type == p_block->expected_type) {
						correct_constant_expression = true;
					}
				}

				if (!correct_constant_expression) {
					if (p_block->expected_type == TYPE_UINT) {
						_set_error(RTR("Expected an unsigned integer constant."));
					} else {
						_set_error(RTR("Expected an integer constant."));
					}
					return ERR_PARSE_ERROR;
				}

				VariableNode *vn = alloc_node<VariableNode>();
				vn->name = tk.text;
				{
					Vector<Scalar> v;
					DataType data_type;

					_find_identifier(p_block, false, p_function_info, vn->name, &data_type, nullptr, nullptr, nullptr, nullptr, &v);
					if (data_type == TYPE_INT) {
						if (p_block->constants.has(v[0].sint)) {
							_set_error(vformat(RTR("Duplicated case label: %d."), v[0].sint));
							return ERR_PARSE_ERROR;
						}
						p_block->constants.insert(v[0].sint);
					} else {
						if (p_block->constants.has(v[0].uint)) {
							_set_error(vformat(RTR("Duplicated case label: %d."), v[0].uint));
							return ERR_PARSE_ERROR;
						}
						p_block->constants.insert(v[0].uint);
					}
				}
				n = vn;
			} else {
				ConstantNode *cn = alloc_node<ConstantNode>();
				Scalar v;
				if (p_block->expected_type == TYPE_UINT) {
					if (tk.type != TK_UINT_CONSTANT) {
						_set_error(RTR("Expected an unsigned integer constant."));
						return ERR_PARSE_ERROR;
					}
					v.uint = (uint32_t)tk.constant;
					if (p_block->constants.has(v.uint)) {
						_set_error(vformat(RTR("Duplicated case label: %d."), v.uint));
						return ERR_PARSE_ERROR;
					}
					p_block->constants.insert(v.uint);
					cn->datatype = TYPE_UINT;
				} else {
					if (tk.type != TK_INT_CONSTANT) {
						_set_error(RTR("Expected an integer constant."));
						return ERR_PARSE_ERROR;
					}
					v.sint = (int32_t)tk.constant * sign;
					if (p_block->constants.has(v.sint)) {
						_set_error(vformat(RTR("Duplicated case label: %d."), v.sint));
						return ERR_PARSE_ERROR;
					}
					p_block->constants.insert(v.sint);
					cn->datatype = TYPE_INT;
				}
				cn->values.push_back(v);
				n = cn;
			}

			tk = _get_token();

			if (tk.type != TK_COLON) {
				_set_expected_error(":");
				return ERR_PARSE_ERROR;
			}

			ControlFlowNode *cf = alloc_node<ControlFlowNode>();
			cf->flow_op = FLOW_OP_CASE;

			BlockNode *case_block = alloc_node<BlockNode>();
			case_block->block_type = BlockNode::BLOCK_TYPE_CASE;
			case_block->parent_block = p_block;
			cf->expressions.push_back(n);
			cf->blocks.push_back(case_block);
			p_block->statements.push_back(cf);

			Error err = _parse_block(case_block, p_function_info, false, true, false);
			if (err) {
				return err;
			}

			return OK;

		} else if (tk.type == TK_CF_DEFAULT) {
			if (p_block && p_block->block_type == BlockNode::BLOCK_TYPE_CASE) {
				_set_tkpos(pos);
				return OK;
			}

			if (!p_block || (p_block->block_type != BlockNode::BLOCK_TYPE_SWITCH)) {
				_set_error(vformat(RTR("'%s' must be placed within a '%s' block."), "default", "switch"));
				return ERR_PARSE_ERROR;
			}

			tk = _get_token();

			if (tk.type != TK_COLON) {
				_set_expected_error(":");
				return ERR_PARSE_ERROR;
			}

			ControlFlowNode *cf = alloc_node<ControlFlowNode>();
			cf->flow_op = FLOW_OP_DEFAULT;

			BlockNode *default_block = alloc_node<BlockNode>();
			default_block->block_type = BlockNode::BLOCK_TYPE_DEFAULT;
			default_block->parent_block = p_block;
			cf->blocks.push_back(default_block);
			p_block->statements.push_back(cf);

			Error err = _parse_block(default_block, p_function_info, false, true, false);
			if (err) {
				return err;
			}

			return OK;

		} else if (tk.type == TK_CF_DO || tk.type == TK_CF_WHILE) {
			// do {} while()
			// while() {}
			bool is_do = tk.type == TK_CF_DO;

			BlockNode *do_block = nullptr;
			if (is_do) {
				do_block = alloc_node<BlockNode>();
				do_block->parent_block = p_block;

				Error err = _parse_block(do_block, p_function_info, true, true, true);
				if (err) {
					return err;
				}

				tk = _get_token();
				if (tk.type != TK_CF_WHILE) {
					_set_expected_after_error("while", "do");
					return ERR_PARSE_ERROR;
				}
			}
			tk = _get_token();

			if (tk.type != TK_PARENTHESIS_OPEN) {
				_set_expected_after_error("(", "while");
				return ERR_PARSE_ERROR;
			}

			ControlFlowNode *cf = alloc_node<ControlFlowNode>();
			if (is_do) {
				cf->flow_op = FLOW_OP_DO;
			} else {
				cf->flow_op = FLOW_OP_WHILE;
			}
			Node *n = _parse_and_reduce_expression(p_block, p_function_info);
			if (!n) {
				return ERR_PARSE_ERROR;
			}

			tk = _get_token();
			if (tk.type != TK_PARENTHESIS_CLOSE) {
				_set_expected_error(")");
				return ERR_PARSE_ERROR;
			}
			if (!is_do) {
				BlockNode *block = alloc_node<BlockNode>();
				block->parent_block = p_block;
				cf->expressions.push_back(n);
				cf->blocks.push_back(block);
				p_block->statements.push_back(cf);

				Error err = _parse_block(block, p_function_info, true, true, true);
				if (err) {
					return err;
				}
			} else {
				cf->expressions.push_back(n);
				cf->blocks.push_back(do_block);
				p_block->statements.push_back(cf);

				tk = _get_token();
				if (tk.type != TK_SEMICOLON) {
					_set_expected_error(";");
					return ERR_PARSE_ERROR;
				}
			}
		} else if (tk.type == TK_CF_FOR) {
			// for() {}
			tk = _get_token();
			if (tk.type != TK_PARENTHESIS_OPEN) {
				_set_expected_after_error("(", "for");
				return ERR_PARSE_ERROR;
			}

			ControlFlowNode *cf = alloc_node<ControlFlowNode>();
			cf->flow_op = FLOW_OP_FOR;

			BlockNode *init_block = alloc_node<BlockNode>();
			init_block->block_type = BlockNode::BLOCK_TYPE_FOR_INIT;
			init_block->parent_block = p_block;
			init_block->single_statement = true;
			cf->blocks.push_back(init_block);

#ifdef DEBUG_ENABLED
			keyword_completion_context = CF_DATATYPE;
#endif // DEBUG_ENABLED
			Error err = _parse_block(init_block, p_function_info, true, false, false);
			if (err != OK) {
				return err;
			}
#ifdef DEBUG_ENABLED
			keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED

			BlockNode *condition_block = alloc_node<BlockNode>();
			condition_block->block_type = BlockNode::BLOCK_TYPE_FOR_CONDITION;
			condition_block->parent_block = init_block;
			condition_block->single_statement = true;
			condition_block->use_comma_between_statements = true;
			cf->blocks.push_back(condition_block);
			err = _parse_block(condition_block, p_function_info, true, false, false);
			if (err != OK) {
				return err;
			}

			BlockNode *expression_block = alloc_node<BlockNode>();
			expression_block->block_type = BlockNode::BLOCK_TYPE_FOR_EXPRESSION;
			expression_block->parent_block = init_block;
			expression_block->single_statement = true;
			expression_block->use_comma_between_statements = true;
			cf->blocks.push_back(expression_block);
			err = _parse_block(expression_block, p_function_info, true, false, false);
			if (err != OK) {
				return err;
			}

			BlockNode *block = alloc_node<BlockNode>();
			block->parent_block = init_block;
			cf->blocks.push_back(block);
			p_block->statements.push_back(cf);

#ifdef DEBUG_ENABLED
			keyword_completion_context = CF_BLOCK;
#endif // DEBUG_ENABLED
			err = _parse_block(block, p_function_info, true, true, true);
			if (err != OK) {
				return err;
			}

		} else if (tk.type == TK_CF_RETURN) {
			//check return type
			BlockNode *b = p_block;

			while (b && !b->parent_function) {
				b = b->parent_block;
			}

			if (!b) {
				_set_parsing_error();
				return ERR_BUG;
			}

			if (b->parent_function && p_function_info.main_function) {
				_set_error(vformat(RTR("Using '%s' in the '%s' processor function is incorrect."), "return", b->parent_function->name));
				return ERR_PARSE_ERROR;
			}

			String return_struct_name = String(b->parent_function->return_struct_name);
			String array_size_string;

			if (b->parent_function->return_array_size > 0) {
				array_size_string = "[" + itos(b->parent_function->return_array_size) + "]";
			}

			ControlFlowNode *flow = alloc_node<ControlFlowNode>();
			flow->flow_op = FLOW_OP_RETURN;

			pos = _get_tkpos();
			tk = _get_token();
			if (tk.type == TK_SEMICOLON) {
				//all is good
				if (b->parent_function->return_type != TYPE_VOID) {
					_set_error(vformat(RTR("Expected '%s' with an expression of type '%s'."), "return", (!return_struct_name.is_empty() ? return_struct_name : get_datatype_name(b->parent_function->return_type)) + array_size_string));
					return ERR_PARSE_ERROR;
				}
			} else {
				if (b->parent_function->return_type == TYPE_VOID) {
					_set_error(vformat(RTR("'%s' function cannot return a value."), "void"));
					return ERR_PARSE_ERROR;
				}

				_set_tkpos(pos); //rollback, wants expression

#ifdef DEBUG_ENABLED
				if (b->parent_function->return_type == DataType::TYPE_BOOL) {
					keyword_completion_context = CF_BOOLEAN;
				}
#endif // DEBUG_ENABLED

				Node *expr = _parse_and_reduce_expression(p_block, p_function_info);
				if (!expr) {
					return ERR_PARSE_ERROR;
				}

				if (b->parent_function->return_type != expr->get_datatype() || b->parent_function->return_array_size != expr->get_array_size() || return_struct_name != expr->get_datatype_name()) {
					_set_error(vformat(RTR("Expected return with an expression of type '%s'."), (!return_struct_name.is_empty() ? return_struct_name : get_datatype_name(b->parent_function->return_type)) + array_size_string));
					return ERR_PARSE_ERROR;
				}

				tk = _get_token();
				if (tk.type != TK_SEMICOLON) {
					_set_expected_after_error(";", "return");
					return ERR_PARSE_ERROR;
				}

#ifdef DEBUG_ENABLED
				if (b->parent_function->return_type == DataType::TYPE_BOOL) {
					keyword_completion_context = CF_BLOCK;
				}
#endif // DEBUG_ENABLED

				flow->expressions.push_back(expr);
			}

			p_block->statements.push_back(flow);

			BlockNode *block = p_block;
			while (block) {
				if (block->block_type == BlockNode::BLOCK_TYPE_CASE || block->block_type == BlockNode::BLOCK_TYPE_DEFAULT) {
					return OK;
				}
				block = block->parent_block;
			}
		} else if (tk.type == TK_CF_DISCARD) {
			if (!is_discard_supported) {
				_set_error(vformat(RTR("Use of '%s' is not supported for the '%s' shader type."), "discard", shader_type_identifier));
				return ERR_PARSE_ERROR;
			}

			//check return type
			BlockNode *b = p_block;
			while (b && !b->parent_function) {
				b = b->parent_block;
			}
			if (!b) {
				_set_parsing_error();
				return ERR_BUG;
			}

			if (!b->parent_function->can_discard) {
				_set_error(vformat(RTR("'%s' cannot be used within the '%s' processor function."), "discard", b->parent_function->name));
				return ERR_PARSE_ERROR;
			}

			ControlFlowNode *flow = alloc_node<ControlFlowNode>();
			flow->flow_op = FLOW_OP_DISCARD;

			pos = _get_tkpos();
			tk = _get_token();

			calls_info[b->parent_function->name].uses_restricted_items.push_back(Pair<StringName, CallInfo::Item>("discard", CallInfo::Item(CallInfo::Item::ITEM_TYPE_BUILTIN, pos)));

			if (tk.type != TK_SEMICOLON) {
				_set_expected_after_error(";", "discard");
				return ERR_PARSE_ERROR;
			}

			p_block->statements.push_back(flow);
		} else if (tk.type == TK_CF_BREAK) {
			if (!p_can_break) {
				_set_error(vformat(RTR("'%s' is not allowed outside of a loop or '%s' statement."), "break", "switch"));
				return ERR_PARSE_ERROR;
			}

			ControlFlowNode *flow = alloc_node<ControlFlowNode>();
			flow->flow_op = FLOW_OP_BREAK;

			pos = _get_tkpos();
			tk = _get_token();
			if (tk.type != TK_SEMICOLON) {
				_set_expected_after_error(";", "break");
				return ERR_PARSE_ERROR;
			}

			p_block->statements.push_back(flow);

			BlockNode *block = p_block;
			while (block) {
				if (block->block_type == BlockNode::BLOCK_TYPE_CASE || block->block_type == BlockNode::BLOCK_TYPE_DEFAULT) {
					return OK;
				}
				block = block->parent_block;
			}

		} else if (tk.type == TK_CF_CONTINUE) {
			if (!p_can_continue) {
				_set_error(vformat(RTR("'%s' is not allowed outside of a loop."), "continue"));
				return ERR_PARSE_ERROR;
			}

			ControlFlowNode *flow = alloc_node<ControlFlowNode>();
			flow->flow_op = FLOW_OP_CONTINUE;

			pos = _get_tkpos();
			tk = _get_token();
			if (tk.type != TK_SEMICOLON) {
				//all is good
				_set_expected_after_error(";", "continue");
				return ERR_PARSE_ERROR;
			}

			p_block->statements.push_back(flow);

		} else {
			//nothing else, so expression
			_set_tkpos(pos); //rollback
			Node *expr = _parse_and_reduce_expression(p_block, p_function_info);
			if (!expr) {
				return ERR_PARSE_ERROR;
			}
			is_condition = expr->get_datatype() == TYPE_BOOL;

			if (expr->type == Node::NODE_TYPE_OPERATOR) {
				OperatorNode *op = static_cast<OperatorNode *>(expr);
				if (op->op == OP_EMPTY) {
					is_var_init = true;
					is_condition = true;
				}
			}

			p_block->statements.push_back(expr);
			tk = _get_token();

			if (p_block->block_type == BlockNode::BLOCK_TYPE_FOR_CONDITION) {
				if (tk.type == TK_COMMA) {
					if (!is_condition) {
						_set_error(RTR("The middle expression is expected to have a boolean data type."));
						return ERR_PARSE_ERROR;
					}
					tk = _peek();
					if (tk.type == TK_SEMICOLON) {
						_set_error(vformat(RTR("Expected expression, found: '%s'."), get_token_text(tk)));
						return ERR_PARSE_ERROR;
					}
					continue;
				}
				if (tk.type != TK_SEMICOLON) {
					_set_expected_error(",", ";");
					return ERR_PARSE_ERROR;
				}
			} else if (p_block->block_type == BlockNode::BLOCK_TYPE_FOR_EXPRESSION) {
				if (tk.type == TK_COMMA) {
					tk = _peek();
					if (tk.type == TK_PARENTHESIS_CLOSE) {
						_set_error(vformat(RTR("Expected expression, found: '%s'."), get_token_text(tk)));
						return ERR_PARSE_ERROR;
					}
					continue;
				}
				if (tk.type != TK_PARENTHESIS_CLOSE) {
					_set_expected_error(",", ")");
					return ERR_PARSE_ERROR;
				}
			} else if (tk.type != TK_SEMICOLON) {
				_set_expected_error(";");
				return ERR_PARSE_ERROR;
			}
		}

		if (p_block) {
			if (p_block->block_type == BlockNode::BLOCK_TYPE_FOR_INIT && !is_var_init) {
				_set_error(RTR("The left expression is expected to be a variable declaration."));
				return ERR_PARSE_ERROR;
			}
			if (p_block->block_type == BlockNode::BLOCK_TYPE_FOR_CONDITION && !is_condition) {
				_set_error(RTR("The middle expression is expected to have a boolean data type."));
				return ERR_PARSE_ERROR;
			}
		}

		if (p_just_one) {
			break;
		}
	}

	return OK;
}

String ShaderLanguage::_get_shader_type_list(const HashSet<String> &p_shader_types) const {
	// Return a list of shader types as an human-readable string
	String valid_types;
	for (const String &E : p_shader_types) {
		if (!valid_types.is_empty()) {
			valid_types += ", ";
		}

		valid_types += "'" + E + "'";
	}

	return valid_types;
}

String ShaderLanguage::_get_qualifier_str(ArgumentQualifier p_qualifier) const {
	switch (p_qualifier) {
		case ArgumentQualifier::ARGUMENT_QUALIFIER_IN:
			return "in";
		case ArgumentQualifier::ARGUMENT_QUALIFIER_OUT:
			return "out";
		case ArgumentQualifier::ARGUMENT_QUALIFIER_INOUT:
			return "inout";
	}
	return "";
}

Error ShaderLanguage::_validate_precision(DataType p_type, DataPrecision p_precision) {
	switch (p_type) {
		case TYPE_STRUCT: {
			_set_error(RTR("The precision modifier cannot be used on structs."));
			return FAILED;
		} break;
		case TYPE_BOOL:
		case TYPE_BVEC2:
		case TYPE_BVEC3:
		case TYPE_BVEC4: {
			_set_error(RTR("The precision modifier cannot be used on boolean types."));
			return FAILED;
		} break;
		default:
			break;
	}
	return OK;
}

Error ShaderLanguage::_parse_shader(const HashMap<StringName, FunctionInfo> &p_functions, const Vector<ModeInfo> &p_render_modes, const HashSet<String> &p_shader_types) {
	Token tk;
	TkPos prev_pos;
	Token next;

	if (!is_shader_inc) {
#ifdef DEBUG_ENABLED
		keyword_completion_context = CF_SHADER_TYPE;
#endif // DEBUG_ENABLED
		tk = _get_token();

		if (tk.type != TK_SHADER_TYPE) {
			_set_error(vformat(RTR("Expected '%s' at the beginning of shader. Valid types are: %s."), "shader_type", _get_shader_type_list(p_shader_types)));
			return ERR_PARSE_ERROR;
		}
#ifdef DEBUG_ENABLED
		keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED

		_get_completable_identifier(nullptr, COMPLETION_SHADER_TYPE, shader_type_identifier);
		if (shader_type_identifier == StringName()) {
			_set_error(vformat(RTR("Expected an identifier after '%s', indicating the type of shader. Valid types are: %s."), "shader_type", _get_shader_type_list(p_shader_types)));
			return ERR_PARSE_ERROR;
		}
		if (!p_shader_types.has(shader_type_identifier)) {
			_set_error(vformat(RTR("Invalid shader type. Valid types are: %s"), _get_shader_type_list(p_shader_types)));
			return ERR_PARSE_ERROR;
		}
		prev_pos = _get_tkpos();
		tk = _get_token();

		if (tk.type != TK_SEMICOLON) {
			_set_tkpos(prev_pos);
			_set_expected_after_error(";", "shader_type " + String(shader_type_identifier));
			return ERR_PARSE_ERROR;
		}
	}

#ifdef DEBUG_ENABLED
	keyword_completion_context = CF_GLOBAL_SPACE;
#endif // DEBUG_ENABLED
	tk = _get_token();

	int texture_uniforms = 0;
	int texture_binding = 0;
	int uniforms = 0;
	int instance_index = 0;
	int prop_index = 0;
#ifdef DEBUG_ENABLED
	uint64_t uniform_buffer_size = 0;
	uint64_t max_uniform_buffer_size = 0;
	int uniform_buffer_exceeded_line = -1;

	bool check_device_limit_warnings = false;
	{
		RenderingDevice *device = RenderingDevice::get_singleton();
		if (device != nullptr) {
			check_device_limit_warnings = check_warnings && HAS_WARNING(ShaderWarning::DEVICE_LIMIT_EXCEEDED_FLAG);

			max_uniform_buffer_size = device->limit_get(RenderingDevice::LIMIT_MAX_UNIFORM_BUFFER_SIZE);
		}
	}
#endif // DEBUG_ENABLED
	ShaderNode::Uniform::Scope uniform_scope = ShaderNode::Uniform::SCOPE_LOCAL;

	stages = &p_functions;

	is_discard_supported = shader_type_identifier == "canvas_item" || shader_type_identifier == "spatial";
	is_supported_frag_only_funcs = is_discard_supported || shader_type_identifier == "sky";

	const FunctionInfo &constants = p_functions.has("constants") ? p_functions["constants"] : FunctionInfo();

	HashMap<String, String> defined_modes;

	while (tk.type != TK_EOF) {
		switch (tk.type) {
			case TK_RENDER_MODE: {
#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED
				while (true) {
					StringName mode;
					_get_completable_identifier(nullptr, COMPLETION_RENDER_MODE, mode);

					if (mode == StringName()) {
						_set_error(RTR("Expected an identifier for render mode."));
						return ERR_PARSE_ERROR;
					}

					const String smode = String(mode);

					if (shader->render_modes.has(mode)) {
						_set_error(vformat(RTR("Duplicated render mode: '%s'."), smode));
						return ERR_PARSE_ERROR;
					}

					bool found = false;

					if (is_shader_inc) {
						for (int i = 0; i < RenderingServer::SHADER_MAX; i++) {
							const Vector<ModeInfo> modes = ShaderTypes::get_singleton()->get_modes(RenderingServer::ShaderMode(i));

							for (int j = 0; j < modes.size(); j++) {
								const ModeInfo &info = modes[j];
								const String name = String(info.name);

								if (smode.begins_with(name)) {
									if (!info.options.is_empty()) {
										if (info.options.has(smode.substr(name.length() + 1))) {
											found = true;

											if (defined_modes.has(name)) {
												_set_error(vformat(RTR("Redefinition of render mode: '%s'. The '%s' mode has already been set to '%s'."), smode, name, defined_modes[name]));
												return ERR_PARSE_ERROR;
											}
											defined_modes.insert(name, smode);
											break;
										}
									} else {
										found = true;
										break;
									}
								}
							}
						}
					} else {
						for (int i = 0; i < p_render_modes.size(); i++) {
							const ModeInfo &info = p_render_modes[i];
							const String name = String(info.name);

							if (smode.begins_with(name)) {
								if (!info.options.is_empty()) {
									if (info.options.has(smode.substr(name.length() + 1))) {
										found = true;

										if (defined_modes.has(name)) {
											_set_error(vformat(RTR("Redefinition of render mode: '%s'. The '%s' mode has already been set to '%s'."), smode, name, defined_modes[name]));
											return ERR_PARSE_ERROR;
										}
										defined_modes.insert(name, smode);
										break;
									}
								} else {
									found = true;
									break;
								}
							}
						}
					}

					if (!found) {
						_set_error(vformat(RTR("Invalid render mode: '%s'."), smode));
						return ERR_PARSE_ERROR;
					}

					shader->render_modes.push_back(mode);

					tk = _get_token();
					if (tk.type == TK_COMMA) {
						//all good, do nothing
					} else if (tk.type == TK_SEMICOLON) {
						break; //done
					} else {
						_set_error(vformat(RTR("Unexpected token: '%s'."), get_token_text(tk)));
						return ERR_PARSE_ERROR;
					}
				}
#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_GLOBAL_SPACE;
#endif // DEBUG_ENABLED
			} break;
			case TK_STRUCT: {
				ShaderNode::Struct st;
				DataType type;
#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED
				tk = _get_token();
				if (tk.type == TK_IDENTIFIER) {
					st.name = tk.text;
					if (shader->constants.has(st.name) || shader->structs.has(st.name)) {
						_set_redefinition_error(String(st.name));
						return ERR_PARSE_ERROR;
					}
					tk = _get_token();
					if (tk.type != TK_CURLY_BRACKET_OPEN) {
						_set_expected_error("{");
						return ERR_PARSE_ERROR;
					}
				} else {
					_set_error(RTR("Expected a struct identifier."));
					return ERR_PARSE_ERROR;
				}

				StructNode *st_node = alloc_node<StructNode>();
				st.shader_struct = st_node;

				int member_count = 0;
				HashSet<String> member_names;

				while (true) { // variables list
#ifdef DEBUG_ENABLED
					keyword_completion_context = CF_DATATYPE | CF_PRECISION_MODIFIER;
#endif // DEBUG_ENABLED

					tk = _get_token();
					if (tk.type == TK_CURLY_BRACKET_CLOSE) {
						break;
					}
					StringName struct_name = "";
					bool struct_dt = false;
					DataPrecision precision = PRECISION_DEFAULT;

					if (tk.type == TK_STRUCT) {
						_set_error(RTR("Nested structs are not allowed."));
						return ERR_PARSE_ERROR;
					}

					if (is_token_precision(tk.type)) {
						precision = get_token_precision(tk.type);
						tk = _get_token();
#ifdef DEBUG_ENABLED
						keyword_completion_context ^= CF_PRECISION_MODIFIER;
#endif // DEBUG_ENABLED
					}

					if (shader->structs.has(tk.text)) {
						struct_name = tk.text;
#ifdef DEBUG_ENABLED
						if (check_warnings && HAS_WARNING(ShaderWarning::UNUSED_STRUCT_FLAG) && used_structs.has(struct_name)) {
							used_structs[struct_name].used = true;
						}
#endif // DEBUG_ENABLED
						struct_dt = true;
					}

					if (!is_token_datatype(tk.type) && !struct_dt) {
						_set_error(RTR("Expected data type."));
						return ERR_PARSE_ERROR;
					} else {
						type = struct_dt ? TYPE_STRUCT : get_token_datatype(tk.type);

						if (precision != PRECISION_DEFAULT && _validate_precision(type, precision) != OK) {
							return ERR_PARSE_ERROR;
						}

						if (type == TYPE_VOID || is_sampler_type(type)) {
							_set_error(vformat(RTR("A '%s' data type is not allowed here."), get_datatype_name(type)));
							return ERR_PARSE_ERROR;
						}
#ifdef DEBUG_ENABLED
						keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED

						bool first = true;
						bool fixed_array_size = false;
						int array_size = 0;

						do {
							tk = _get_token();

							if (first) {
								first = false;

								if (tk.type != TK_IDENTIFIER && tk.type != TK_BRACKET_OPEN) {
									_set_error(RTR("Expected an identifier or '['."));
									return ERR_PARSE_ERROR;
								}

								if (tk.type == TK_BRACKET_OPEN) {
									Error error = _parse_array_size(nullptr, constants, true, nullptr, &array_size, nullptr);
									if (error != OK) {
										return error;
									}
									fixed_array_size = true;
									tk = _get_token();
								}
							}

							if (tk.type != TK_IDENTIFIER) {
								_set_error(RTR("Expected an identifier."));
								return ERR_PARSE_ERROR;
							}

							MemberNode *member = alloc_node<MemberNode>();
							member->precision = precision;
							member->datatype = type;
							member->struct_name = struct_name;
							member->name = tk.text;
							member->array_size = array_size;

							if (member_names.has(member->name)) {
								_set_redefinition_error(String(member->name));
								return ERR_PARSE_ERROR;
							}
							member_names.insert(member->name);
							tk = _get_token();

							if (tk.type == TK_BRACKET_OPEN) {
								Error error = _parse_array_size(nullptr, constants, true, nullptr, &member->array_size, nullptr);
								if (error != OK) {
									return error;
								}
								tk = _get_token();
							}

							if (!fixed_array_size) {
								array_size = 0;
							}

							if (tk.type != TK_SEMICOLON && tk.type != TK_COMMA) {
								_set_expected_error(",", ";");
								return ERR_PARSE_ERROR;
							}

							st_node->members.push_back(member);
							member_count++;
						} while (tk.type == TK_COMMA); // another member
					}
				}
				if (member_count == 0) {
					_set_error(RTR("Empty structs are not allowed."));
					return ERR_PARSE_ERROR;
				}
#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED

				tk = _get_token();
				if (tk.type != TK_SEMICOLON) {
					_set_expected_error(";");
					return ERR_PARSE_ERROR;
				}
#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_GLOBAL_SPACE;
#endif // DEBUG_ENABLED

				shader->structs[st.name] = st;
				shader->vstructs.push_back(st); // struct's order is important!
#ifdef DEBUG_ENABLED
				if (check_warnings && HAS_WARNING(ShaderWarning::UNUSED_STRUCT_FLAG)) {
					used_structs.insert(st.name, Usage(tk_line));
				}
#endif // DEBUG_ENABLED
			} break;
			case TK_GLOBAL: {
#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_UNIFORM_KEYWORD;
				if (_lookup_next(next)) {
					if (next.type == TK_UNIFORM) {
						keyword_completion_context ^= CF_UNIFORM_KEYWORD;
					}
				}
#endif // DEBUG_ENABLED
				tk = _get_token();
				if (tk.type != TK_UNIFORM) {
					_set_expected_after_error("uniform", "global");
					return ERR_PARSE_ERROR;
				}
				uniform_scope = ShaderNode::Uniform::SCOPE_GLOBAL;
			};
				[[fallthrough]];
			case TK_INSTANCE: {
				if (tk.type == TK_INSTANCE) {
#ifdef DEBUG_ENABLED
					keyword_completion_context = CF_UNIFORM_KEYWORD;
					if (_lookup_next(next)) {
						if (next.type == TK_UNIFORM) {
							keyword_completion_context ^= CF_UNIFORM_KEYWORD;
						}
					}
#endif // DEBUG_ENABLED
					if (shader_type_identifier != StringName() && String(shader_type_identifier) != "spatial") {
						_set_error(vformat(RTR("Uniform instances are not yet implemented for '%s' shaders."), shader_type_identifier));
						return ERR_PARSE_ERROR;
					}
					if (uniform_scope == ShaderNode::Uniform::SCOPE_LOCAL) {
						tk = _get_token();
						if (tk.type != TK_UNIFORM) {
							_set_expected_after_error("uniform", "instance");
							return ERR_PARSE_ERROR;
						}
						uniform_scope = ShaderNode::Uniform::SCOPE_INSTANCE;
					}
				}
			};
				[[fallthrough]];
			case TK_UNIFORM:
			case TK_VARYING: {
				bool is_uniform = tk.type == TK_UNIFORM;
#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED
				if (!is_uniform) {
					if (shader_type_identifier == "particles" || shader_type_identifier == "sky" || shader_type_identifier == "fog") {
						_set_error(vformat(RTR("Varyings cannot be used in '%s' shaders."), shader_type_identifier));
						return ERR_PARSE_ERROR;
					}
				}
				DataPrecision precision = PRECISION_DEFAULT;
				DataInterpolation interpolation = INTERPOLATION_DEFAULT;
				DataType type;
				StringName name;
				int array_size = 0;

				tk = _get_token();
#ifdef DEBUG_ENABLED
				bool temp_error = false;
				uint32_t datatype_flag;

				if (!is_uniform) {
					datatype_flag = CF_VARYING_TYPE;
					keyword_completion_context = CF_INTERPOLATION_QUALIFIER | CF_PRECISION_MODIFIER | datatype_flag;

					if (_lookup_next(next)) {
						if (is_token_interpolation(next.type)) {
							keyword_completion_context ^= (CF_INTERPOLATION_QUALIFIER | datatype_flag);
						} else if (is_token_precision(next.type)) {
							keyword_completion_context ^= (CF_PRECISION_MODIFIER | datatype_flag);
						} else if (is_token_datatype(next.type)) {
							keyword_completion_context ^= datatype_flag;
						}
					}
				} else {
					datatype_flag = CF_UNIFORM_TYPE;
					keyword_completion_context = CF_PRECISION_MODIFIER | datatype_flag;

					if (_lookup_next(next)) {
						if (is_token_precision(next.type)) {
							keyword_completion_context ^= (CF_PRECISION_MODIFIER | datatype_flag);
						} else if (is_token_datatype(next.type)) {
							keyword_completion_context ^= datatype_flag;
						}
					}
				}
#endif // DEBUG_ENABLED

				if (is_token_interpolation(tk.type)) {
					if (is_uniform) {
						_set_error(RTR("Interpolation qualifiers are not supported for uniforms."));
#ifdef DEBUG_ENABLED
						temp_error = true;
#else
						return ERR_PARSE_ERROR;
#endif // DEBUG_ENABLED
					}
					interpolation = get_token_interpolation(tk.type);
					tk = _get_token();
#ifdef DEBUG_ENABLED
					if (keyword_completion_context & CF_INTERPOLATION_QUALIFIER) {
						keyword_completion_context ^= CF_INTERPOLATION_QUALIFIER;
					}
					if (_lookup_next(next)) {
						if (is_token_precision(next.type)) {
							keyword_completion_context ^= CF_PRECISION_MODIFIER;
						} else if (is_token_datatype(next.type)) {
							keyword_completion_context ^= datatype_flag;
						}
					}
					if (temp_error) {
						return ERR_PARSE_ERROR;
					}
#endif // DEBUG_ENABLED
				}

				if (is_token_precision(tk.type)) {
					precision = get_token_precision(tk.type);
					tk = _get_token();
#ifdef DEBUG_ENABLED
					if (keyword_completion_context & CF_INTERPOLATION_QUALIFIER) {
						keyword_completion_context ^= CF_INTERPOLATION_QUALIFIER;
					}
					if (keyword_completion_context & CF_PRECISION_MODIFIER) {
						keyword_completion_context ^= CF_PRECISION_MODIFIER;
					}
					if (_lookup_next(next)) {
						if (is_token_datatype(next.type)) {
							keyword_completion_context = CF_UNSPECIFIED;
						}
					}
#endif // DEBUG_ENABLED
				}

				if (shader->structs.has(tk.text)) {
					if (is_uniform) {
						_set_error(vformat(RTR("The '%s' data type is not supported for uniforms."), "struct"));
						return ERR_PARSE_ERROR;
					} else {
						_set_error(vformat(RTR("The '%s' data type is not allowed here."), "struct"));
						return ERR_PARSE_ERROR;
					}
				}

				if (!is_token_datatype(tk.type)) {
					_set_error(RTR("Expected data type."));
					return ERR_PARSE_ERROR;
				}

				type = get_token_datatype(tk.type);

				if (precision != PRECISION_DEFAULT && _validate_precision(type, precision) != OK) {
					return ERR_PARSE_ERROR;
				}

				if (type == TYPE_VOID) {
					_set_error(vformat(RTR("The '%s' data type is not allowed here."), "void"));
					return ERR_PARSE_ERROR;
				}

				if (!is_uniform && interpolation != INTERPOLATION_DEFAULT && type < TYPE_INT) {
					_set_error(vformat(RTR("Interpolation modifier '%s' cannot be used with boolean types."), get_interpolation_name(interpolation)));
					return ERR_PARSE_ERROR;
				}

				if (!is_uniform && type > TYPE_MAT4) {
					_set_error(RTR("Invalid data type for varying."));
					return ERR_PARSE_ERROR;
				}

#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED
				tk = _get_token();

				if (tk.type != TK_IDENTIFIER && tk.type != TK_BRACKET_OPEN) {
					_set_error(RTR("Expected an identifier or '['."));
					return ERR_PARSE_ERROR;
				}

				if (tk.type == TK_BRACKET_OPEN) {
					Error error = _parse_array_size(nullptr, constants, true, nullptr, &array_size, nullptr);
					if (error != OK) {
						return error;
					}
					tk = _get_token();
				}

				if (tk.type != TK_IDENTIFIER) {
					_set_error(RTR("Expected an identifier."));
					return ERR_PARSE_ERROR;
				}

				prev_pos = _get_tkpos();
				name = tk.text;

				if (_find_identifier(nullptr, false, constants, name)) {
					_set_redefinition_error(String(name));
					return ERR_PARSE_ERROR;
				}

				if (has_builtin(p_functions, name)) {
					_set_redefinition_error(String(name));
					return ERR_PARSE_ERROR;
				}

				if (is_uniform) {
					if (uniform_scope == ShaderNode::Uniform::SCOPE_GLOBAL && Engine::get_singleton()->is_editor_hint()) { // Type checking for global uniforms is not allowed outside the editor.
						//validate global uniform
						DataType gvtype = global_shader_uniform_get_type_func(name);
						if (gvtype == TYPE_MAX) {
							_set_error(vformat(RTR("Global uniform '%s' does not exist. Create it in Project Settings."), String(name)));
							return ERR_PARSE_ERROR;
						}

						if (type != gvtype) {
							_set_error(vformat(RTR("Global uniform '%s' must be of type '%s'."), String(name), get_datatype_name(gvtype)));
							return ERR_PARSE_ERROR;
						}
					}
					ShaderNode::Uniform uniform;

					uniform.type = type;
					uniform.scope = uniform_scope;
					uniform.precision = precision;
					uniform.array_size = array_size;
					uniform.group = current_uniform_group_name;
					uniform.subgroup = current_uniform_subgroup_name;

					tk = _get_token();
					if (tk.type == TK_BRACKET_OPEN) {
						Error error = _parse_array_size(nullptr, constants, true, nullptr, &uniform.array_size, nullptr);
						if (error != OK) {
							return error;
						}
						tk = _get_token();
					}

					if (is_sampler_type(type)) {
						if (uniform_scope == ShaderNode::Uniform::SCOPE_INSTANCE) {
							_set_error(vformat(RTR("The '%s' qualifier is not supported for sampler types."), "SCOPE_INSTANCE"));
							return ERR_PARSE_ERROR;
						}
						uniform.texture_order = texture_uniforms++;
						uniform.texture_binding = texture_binding;
						if (uniform.array_size > 0) {
							texture_binding += uniform.array_size;
						} else {
							++texture_binding;
						}
						uniform.order = -1;
						uniform.prop_order = prop_index++;
					} else {
						if (uniform_scope == ShaderNode::Uniform::SCOPE_INSTANCE && (type == TYPE_MAT2 || type == TYPE_MAT3 || type == TYPE_MAT4)) {
							_set_error(vformat(RTR("The '%s' qualifier is not supported for matrix types."), "SCOPE_INSTANCE"));
							return ERR_PARSE_ERROR;
						}
						uniform.texture_order = -1;
						if (uniform_scope != ShaderNode::Uniform::SCOPE_INSTANCE) {
							uniform.order = uniforms++;
							uniform.prop_order = prop_index++;
#ifdef DEBUG_ENABLED
							if (check_device_limit_warnings) {
								if (uniform.array_size > 0) {
									int size = get_datatype_size(uniform.type) * uniform.array_size;
									int m = (16 * uniform.array_size);
									if ((size % m) != 0U) {
										size += m - (size % m);
									}
									uniform_buffer_size += size;
								} else {
									uniform_buffer_size += get_datatype_size(uniform.type);
								}

								if (uniform_buffer_exceeded_line == -1 && uniform_buffer_size > max_uniform_buffer_size) {
									uniform_buffer_exceeded_line = tk_line;
								}
							}
#endif // DEBUG_ENABLED
						}
					}

					if (uniform.array_size > 0) {
						if (uniform_scope == ShaderNode::Uniform::SCOPE_GLOBAL) {
							_set_error(vformat(RTR("The '%s' qualifier is not supported for uniform arrays."), "SCOPE_GLOBAL"));
							return ERR_PARSE_ERROR;
						}
						if (uniform_scope == ShaderNode::Uniform::SCOPE_INSTANCE) {
							_set_error(vformat(RTR("The '%s' qualifier is not supported for uniform arrays."), "SCOPE_INSTANCE"));
							return ERR_PARSE_ERROR;
						}
					}

					int custom_instance_index = -1;

					if (tk.type == TK_COLON) {
						completion_type = COMPLETION_HINT;
						completion_base = type;
						completion_base_array = uniform.array_size > 0;

						//hint
						do {
							tk = _get_token();
							completion_line = tk.line;

							if (!is_token_hint(tk.type)) {
								_set_error(RTR("Expected valid type hint after ':'."));
								return ERR_PARSE_ERROR;
							}

							if (uniform.array_size > 0) {
								static Vector<int> supported_hints = {
									TK_HINT_SOURCE_COLOR, TK_REPEAT_DISABLE, TK_REPEAT_ENABLE,
									TK_FILTER_LINEAR, TK_FILTER_LINEAR_MIPMAP, TK_FILTER_LINEAR_MIPMAP_ANISOTROPIC,
									TK_FILTER_NEAREST, TK_FILTER_NEAREST_MIPMAP, TK_FILTER_NEAREST_MIPMAP_ANISOTROPIC
								};
								if (!supported_hints.has(tk.type)) {
									_set_error(RTR("This hint is not supported for uniform arrays."));
									return ERR_PARSE_ERROR;
								}
							}

							ShaderNode::Uniform::Hint new_hint = ShaderNode::Uniform::HINT_NONE;
							TextureFilter new_filter = FILTER_DEFAULT;
							TextureRepeat new_repeat = REPEAT_DEFAULT;

							switch (tk.type) {
								case TK_HINT_SOURCE_COLOR: {
									if (type != TYPE_VEC3 && type != TYPE_VEC4 && !is_sampler_type(type)) {
										_set_error(vformat(RTR("Source color hint is for '%s', '%s' or sampler types only."), "vec3", "vec4"));
										return ERR_PARSE_ERROR;
									}

									if (is_sampler_type(type)) {
										if (uniform.use_color) {
											_set_error(vformat(RTR("Duplicated hint: '%s'."), "source_color"));
											return ERR_PARSE_ERROR;
										}
										uniform.use_color = true;
									} else {
										new_hint = ShaderNode::Uniform::HINT_SOURCE_COLOR;
									}
								} break;
								case TK_HINT_DEFAULT_BLACK_TEXTURE: {
									new_hint = ShaderNode::Uniform::HINT_DEFAULT_BLACK;
								} break;
								case TK_HINT_DEFAULT_WHITE_TEXTURE: {
									new_hint = ShaderNode::Uniform::HINT_DEFAULT_WHITE;
								} break;
								case TK_HINT_DEFAULT_TRANSPARENT_TEXTURE: {
									new_hint = ShaderNode::Uniform::HINT_DEFAULT_TRANSPARENT;
								} break;
								case TK_HINT_NORMAL_TEXTURE: {
									new_hint = ShaderNode::Uniform::HINT_NORMAL;
								} break;
								case TK_HINT_ROUGHNESS_NORMAL_TEXTURE: {
									new_hint = ShaderNode::Uniform::HINT_ROUGHNESS_NORMAL;
								} break;
								case TK_HINT_ROUGHNESS_R: {
									new_hint = ShaderNode::Uniform::HINT_ROUGHNESS_R;
								} break;
								case TK_HINT_ROUGHNESS_G: {
									new_hint = ShaderNode::Uniform::HINT_ROUGHNESS_G;
								} break;
								case TK_HINT_ROUGHNESS_B: {
									new_hint = ShaderNode::Uniform::HINT_ROUGHNESS_B;
								} break;
								case TK_HINT_ROUGHNESS_A: {
									new_hint = ShaderNode::Uniform::HINT_ROUGHNESS_A;
								} break;
								case TK_HINT_ROUGHNESS_GRAY: {
									new_hint = ShaderNode::Uniform::HINT_ROUGHNESS_GRAY;
								} break;
								case TK_HINT_ANISOTROPY_TEXTURE: {
									new_hint = ShaderNode::Uniform::HINT_ANISOTROPY;
								} break;
								case TK_HINT_RANGE: {
									if (type != TYPE_FLOAT && type != TYPE_INT) {
										_set_error(vformat(RTR("Range hint is for '%s' and '%s' only."), "float", "int"));
										return ERR_PARSE_ERROR;
									}

									tk = _get_token();
									if (tk.type != TK_PARENTHESIS_OPEN) {
										_set_expected_after_error("(", "hint_range");
										return ERR_PARSE_ERROR;
									}

									tk = _get_token();

									float sign = 1.0;

									if (tk.type == TK_OP_SUB) {
										sign = -1.0;
										tk = _get_token();
									}

									if (tk.type != TK_FLOAT_CONSTANT && !tk.is_integer_constant()) {
										_set_error(RTR("Expected an integer constant."));
										return ERR_PARSE_ERROR;
									}

									uniform.hint_range[0] = tk.constant;
									uniform.hint_range[0] *= sign;

									tk = _get_token();

									if (tk.type != TK_COMMA) {
										_set_error(RTR("Expected ',' after integer constant."));
										return ERR_PARSE_ERROR;
									}

									tk = _get_token();

									sign = 1.0;

									if (tk.type == TK_OP_SUB) {
										sign = -1.0;
										tk = _get_token();
									}

									if (tk.type != TK_FLOAT_CONSTANT && !tk.is_integer_constant()) {
										_set_error(RTR("Expected an integer constant after ','."));
										return ERR_PARSE_ERROR;
									}

									uniform.hint_range[1] = tk.constant;
									uniform.hint_range[1] *= sign;

									tk = _get_token();

									if (tk.type == TK_COMMA) {
										tk = _get_token();

										if (tk.type != TK_FLOAT_CONSTANT && !tk.is_integer_constant()) {
											_set_error(RTR("Expected an integer constant after ','."));
											return ERR_PARSE_ERROR;
										}

										uniform.hint_range[2] = tk.constant;
										tk = _get_token();
									} else {
										if (type == TYPE_INT) {
											uniform.hint_range[2] = 1;
										} else {
											uniform.hint_range[2] = 0.001;
										}
									}

									if (tk.type != TK_PARENTHESIS_CLOSE) {
										_set_expected_error(")");
										return ERR_PARSE_ERROR;
									}

									new_hint = ShaderNode::Uniform::HINT_RANGE;
								} break;
								case TK_HINT_ENUM: {
									if (type != TYPE_INT) {
										_set_error(vformat(RTR("Enum hint is for '%s' only."), "int"));
										return ERR_PARSE_ERROR;
									}

									tk = _get_token();
									if (tk.type != TK_PARENTHESIS_OPEN) {
										_set_expected_after_error("(", "hint_enum");
										return ERR_PARSE_ERROR;
									}

									while (true) {
										tk = _get_token();

										if (tk.type != TK_STRING_CONSTANT) {
											_set_error(RTR("Expected a string constant."));
											return ERR_PARSE_ERROR;
										}

										uniform.hint_enum_names.push_back(tk.text);

										tk = _get_token();

										if (tk.type == TK_PARENTHESIS_CLOSE) {
											break;
										} else if (tk.type != TK_COMMA) {
											_set_error(RTR("Expected ',' or ')' after string constant."));
											return ERR_PARSE_ERROR;
										}
									}

									new_hint = ShaderNode::Uniform::HINT_ENUM;
								} break;
								case TK_HINT_INSTANCE_INDEX: {
									if (custom_instance_index != -1) {
										_set_error(vformat(RTR("Can only specify '%s' once."), "instance_index"));
										return ERR_PARSE_ERROR;
									}

									tk = _get_token();
									if (tk.type != TK_PARENTHESIS_OPEN) {
										_set_expected_after_error("(", "instance_index");
										return ERR_PARSE_ERROR;
									}

									tk = _get_token();

									if (tk.type == TK_OP_SUB) {
										_set_error(RTR("The instance index can't be negative."));
										return ERR_PARSE_ERROR;
									}

									if (!tk.is_integer_constant()) {
										_set_error(RTR("Expected an integer constant."));
										return ERR_PARSE_ERROR;
									}

									custom_instance_index = tk.constant;
									current_uniform_instance_index_defined = true;

									if (custom_instance_index >= MAX_INSTANCE_UNIFORM_INDICES) {
										_set_error(vformat(RTR("Allowed instance uniform indices must be within [0..%d] range."), MAX_INSTANCE_UNIFORM_INDICES - 1));
										return ERR_PARSE_ERROR;
									}

									tk = _get_token();

									if (tk.type != TK_PARENTHESIS_CLOSE) {
										_set_expected_error(")");
										return ERR_PARSE_ERROR;
									}
								} break;
								case TK_HINT_SCREEN_TEXTURE: {
									new_hint = ShaderNode::Uniform::HINT_SCREEN_TEXTURE;
									--texture_uniforms;
									--texture_binding;
								} break;
								case TK_HINT_NORMAL_ROUGHNESS_TEXTURE: {
									new_hint = ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE;
									--texture_uniforms;
									--texture_binding;
									if (OS::get_singleton()->get_current_rendering_method() != "forward_plus") {
										_set_error(RTR("'hint_normal_roughness_texture' is only available when using the Forward+ backend."));
										return ERR_PARSE_ERROR;
									}
									if (shader_type_identifier != StringName() && String(shader_type_identifier) != "spatial") {
										_set_error(vformat(RTR("'hint_normal_roughness_texture' is not supported in '%s' shaders."), shader_type_identifier));
										return ERR_PARSE_ERROR;
									}
								} break;
								case TK_HINT_DEPTH_TEXTURE: {
									new_hint = ShaderNode::Uniform::HINT_DEPTH_TEXTURE;
									--texture_uniforms;
									--texture_binding;
									if (shader_type_identifier != StringName() && String(shader_type_identifier) != "spatial") {
										_set_error(vformat(RTR("'hint_depth_texture' is not supported in '%s' shaders."), shader_type_identifier));
										return ERR_PARSE_ERROR;
									}
								} break;
								case TK_FILTER_NEAREST: {
									new_filter = FILTER_NEAREST;
								} break;
								case TK_FILTER_LINEAR: {
									new_filter = FILTER_LINEAR;
								} break;
								case TK_FILTER_NEAREST_MIPMAP: {
									new_filter = FILTER_NEAREST_MIPMAP;
								} break;
								case TK_FILTER_LINEAR_MIPMAP: {
									new_filter = FILTER_LINEAR_MIPMAP;
								} break;
								case TK_FILTER_NEAREST_MIPMAP_ANISOTROPIC: {
									new_filter = FILTER_NEAREST_MIPMAP_ANISOTROPIC;
								} break;
								case TK_FILTER_LINEAR_MIPMAP_ANISOTROPIC: {
									new_filter = FILTER_LINEAR_MIPMAP_ANISOTROPIC;
								} break;
								case TK_REPEAT_DISABLE: {
									new_repeat = REPEAT_DISABLE;
								} break;
								case TK_REPEAT_ENABLE: {
									new_repeat = REPEAT_ENABLE;
								} break;

								default:
									break;
							}

							bool is_sampler_hint = new_hint != ShaderNode::Uniform::HINT_NONE && new_hint != ShaderNode::Uniform::HINT_SOURCE_COLOR && new_hint != ShaderNode::Uniform::HINT_RANGE && new_hint != ShaderNode::Uniform::HINT_ENUM;
							if (((new_filter != FILTER_DEFAULT || new_repeat != REPEAT_DEFAULT) || is_sampler_hint) && !is_sampler_type(type)) {
								_set_error(RTR("This hint is only for sampler types."));
								return ERR_PARSE_ERROR;
							}

							if (new_hint != ShaderNode::Uniform::HINT_NONE) {
								if (uniform.hint != ShaderNode::Uniform::HINT_NONE) {
									if (uniform.hint == new_hint) {
										_set_error(vformat(RTR("Duplicated hint: '%s'."), get_uniform_hint_name(new_hint)));
									} else {
										_set_error(vformat(RTR("Redefinition of hint: '%s'. The hint has already been set to '%s'."), get_uniform_hint_name(new_hint), get_uniform_hint_name(uniform.hint)));
									}
									return ERR_PARSE_ERROR;
								} else {
									uniform.hint = new_hint;
									current_uniform_hint = new_hint;
								}
							}

							if (new_filter != FILTER_DEFAULT) {
								if (uniform.filter != FILTER_DEFAULT) {
									if (uniform.filter == new_filter) {
										_set_error(vformat(RTR("Duplicated filter mode: '%s'."), get_texture_filter_name(new_filter)));
									} else {
										_set_error(vformat(RTR("Redefinition of filter mode: '%s'. The filter mode has already been set to '%s'."), get_texture_filter_name(new_filter), get_texture_filter_name(uniform.filter)));
									}
									return ERR_PARSE_ERROR;
								} else {
									uniform.filter = new_filter;
									current_uniform_filter = new_filter;
								}
							}

							if (new_repeat != REPEAT_DEFAULT) {
								if (uniform.repeat != REPEAT_DEFAULT) {
									if (uniform.repeat == new_repeat) {
										_set_error(vformat(RTR("Duplicated repeat mode: '%s'."), get_texture_repeat_name(new_repeat)));
									} else {
										_set_error(vformat(RTR("Redefinition of repeat mode: '%s'. The repeat mode has already been set to '%s'."), get_texture_repeat_name(new_repeat), get_texture_repeat_name(uniform.repeat)));
									}
									return ERR_PARSE_ERROR;
								} else {
									uniform.repeat = new_repeat;
									current_uniform_repeat = new_repeat;
								}
							}

							tk = _get_token();

						} while (tk.type == TK_COMMA);
					}

					if (uniform_scope == ShaderNode::Uniform::SCOPE_INSTANCE) {
						if (custom_instance_index >= 0) {
							uniform.instance_index = custom_instance_index;
						} else {
							uniform.instance_index = instance_index++;
							if (instance_index > MAX_INSTANCE_UNIFORM_INDICES) {
								_set_error(vformat(RTR("Too many '%s' uniforms in shader, maximum supported is %d."), "instance", MAX_INSTANCE_UNIFORM_INDICES));
								return ERR_PARSE_ERROR;
							}
						}
					}

					//reset scope for next uniform

					if (tk.type == TK_OP_ASSIGN) {
						if (uniform.array_size > 0) {
							_set_error(RTR("Setting default values to uniform arrays is not supported."));
							return ERR_PARSE_ERROR;
						}

						Node *expr = _parse_and_reduce_expression(nullptr, constants);
						if (!expr) {
							return ERR_PARSE_ERROR;
						}
						if (expr->type != Node::NODE_TYPE_CONSTANT) {
							_set_error(RTR("Expected constant expression after '='."));
							return ERR_PARSE_ERROR;
						}

						ConstantNode *cn = static_cast<ConstantNode *>(expr);

						uniform.default_value.resize(cn->values.size());

						if (!convert_constant(cn, uniform.type, uniform.default_value.ptrw())) {
							_set_error(vformat(RTR("Can't convert constant to '%s'."), get_datatype_name(uniform.type)));
							return ERR_PARSE_ERROR;
						}
						tk = _get_token();
					}

					shader->uniforms[name] = uniform;
#ifdef DEBUG_ENABLED
					if (check_warnings && HAS_WARNING(ShaderWarning::UNUSED_UNIFORM_FLAG)) {
						used_uniforms.insert(name, Usage(tk_line));
					}
#endif // DEBUG_ENABLED

					//reset scope for next uniform
					uniform_scope = ShaderNode::Uniform::SCOPE_LOCAL;

					if (tk.type != TK_SEMICOLON) {
						_set_expected_error(";");
						return ERR_PARSE_ERROR;
					}

#ifdef DEBUG_ENABLED
					keyword_completion_context = CF_GLOBAL_SPACE;
#endif // DEBUG_ENABLED
					completion_type = COMPLETION_NONE;

					current_uniform_hint = ShaderNode::Uniform::HINT_NONE;
					current_uniform_filter = FILTER_DEFAULT;
					current_uniform_repeat = REPEAT_DEFAULT;
					current_uniform_instance_index_defined = false;
				} else { // varying
					ShaderNode::Varying varying;
					varying.type = type;
					varying.precision = precision;
					varying.interpolation = interpolation;
					varying.tkpos = prev_pos;
					varying.array_size = array_size;

					tk = _get_token();
					if (tk.type != TK_SEMICOLON && tk.type != TK_BRACKET_OPEN) {
						if (array_size == 0) {
							_set_expected_error(";", "[");
						} else {
							_set_expected_error(";");
						}
						return ERR_PARSE_ERROR;
					}

					if (tk.type == TK_BRACKET_OPEN) {
						Error error = _parse_array_size(nullptr, constants, true, nullptr, &varying.array_size, nullptr);
						if (error != OK) {
							return error;
						}
						tk = _get_token();
					}

					shader->varyings[name] = varying;
#ifdef DEBUG_ENABLED
					if (check_warnings && HAS_WARNING(ShaderWarning::UNUSED_VARYING_FLAG)) {
						used_varyings.insert(name, Usage(tk_line));
					}
#endif // DEBUG_ENABLED
				}

			} break;
			case TK_UNIFORM_GROUP: {
				tk = _get_token();
				if (tk.type == TK_IDENTIFIER) {
					current_uniform_group_name = tk.text;
					current_uniform_subgroup_name = "";
					tk = _get_token();
					if (tk.type == TK_PERIOD) {
						tk = _get_token();
						if (tk.type == TK_IDENTIFIER) {
							current_uniform_subgroup_name = tk.text;
							tk = _get_token();
							if (tk.type != TK_SEMICOLON) {
								_set_expected_error(";");
								return ERR_PARSE_ERROR;
							}
						} else {
							_set_error(RTR("Expected an uniform subgroup identifier."));
							return ERR_PARSE_ERROR;
						}
					} else if (tk.type != TK_SEMICOLON) {
						_set_expected_error(";", ".");
						return ERR_PARSE_ERROR;
					}
				} else {
					if (tk.type != TK_SEMICOLON) {
						if (current_uniform_group_name.is_empty()) {
							_set_error(RTR("Expected an uniform group identifier."));
						} else {
							_set_error(RTR("Expected an uniform group identifier or `;`."));
						}
						return ERR_PARSE_ERROR;
					} else if (current_uniform_group_name.is_empty()) {
						_set_error(RTR("Group needs to be opened before."));
						return ERR_PARSE_ERROR;
					} else {
						current_uniform_group_name = "";
						current_uniform_subgroup_name = "";
					}
				}
			} break;
			case TK_SHADER_TYPE: {
				_set_error(RTR("Shader type is already defined."));
				return ERR_PARSE_ERROR;
			} break;
			default: {
				//function or constant variable

				bool is_constant = false;
				bool is_struct = false;
				StringName struct_name;
				DataPrecision precision = PRECISION_DEFAULT;
				DataType type;
				StringName name;
				int array_size = 0;

				if (tk.type == TK_CONST) {
					is_constant = true;
					tk = _get_token();
				}

				if (is_token_precision(tk.type)) {
					precision = get_token_precision(tk.type);
					tk = _get_token();
				}

				if (shader->structs.has(tk.text)) {
					is_struct = true;
					struct_name = tk.text;
				} else {
#ifdef DEBUG_ENABLED
					if (_lookup_next(next)) {
						if (next.type == TK_UNIFORM) {
							keyword_completion_context = CF_UNIFORM_QUALIFIER;
						}
					}
#endif // DEBUG_ENABLED
					if (!is_token_datatype(tk.type)) {
						_set_error(RTR("Expected constant, function, uniform or varying."));
						return ERR_PARSE_ERROR;
					}

					if (!is_token_variable_datatype(tk.type)) {
						if (is_constant) {
							_set_error(RTR("Invalid constant type (samplers are not allowed)."));
						} else {
							_set_error(RTR("Invalid function type (samplers are not allowed)."));
						}
						return ERR_PARSE_ERROR;
					}
				}

				if (is_struct) {
					type = TYPE_STRUCT;
				} else {
					type = get_token_datatype(tk.type);
				}

				if (precision != PRECISION_DEFAULT && _validate_precision(type, precision) != OK) {
					return ERR_PARSE_ERROR;
				}

				prev_pos = _get_tkpos();
				tk = _get_token();

#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED

				bool unknown_size = false;
				bool fixed_array_size = false;

				if (tk.type == TK_BRACKET_OPEN) {
					Error error = _parse_array_size(nullptr, constants, !is_constant, nullptr, &array_size, &unknown_size);
					if (error != OK) {
						return error;
					}
					fixed_array_size = true;
					prev_pos = _get_tkpos();
				}

				_set_tkpos(prev_pos);

				_get_completable_identifier(nullptr, COMPLETION_MAIN_FUNCTION, name);

				if (name == StringName()) {
					if (is_constant) {
						_set_error(RTR("Expected an identifier or '[' after type."));
					} else {
						_set_error(RTR("Expected a function name after type."));
					}
					return ERR_PARSE_ERROR;
				}

				IdentifierType itype;
				if (shader->structs.has(name) || (_find_identifier(nullptr, false, constants, name, nullptr, &itype) && itype != IDENTIFIER_FUNCTION) || has_builtin(p_functions, name, !is_constant)) {
					_set_redefinition_error(String(name));
					return ERR_PARSE_ERROR;
				}

				tk = _get_token();
				if (tk.type != TK_PARENTHESIS_OPEN) {
					if (type == TYPE_VOID) {
						_set_error(RTR("Expected '(' after function identifier."));
						return ERR_PARSE_ERROR;
					}

					//variable
					while (true) {
						ShaderNode::Constant constant;
						constant.name = name;
						constant.type = is_struct ? TYPE_STRUCT : type;
						constant.struct_name = struct_name;
						constant.precision = precision;
						constant.initializer = nullptr;
						constant.array_size = array_size;
						is_const_decl = true;

						if (tk.type == TK_BRACKET_OPEN) {
							Error error = _parse_array_size(nullptr, constants, false, nullptr, &constant.array_size, &unknown_size);
							if (error != OK) {
								return error;
							}
							tk = _get_token();
						}

						if (tk.type == TK_OP_ASSIGN) {
							if (!is_constant) {
								_set_error(vformat(RTR("Global non-constant variables are not supported. Expected '%s' keyword before constant definition."), "const"));
								return ERR_PARSE_ERROR;
							}

							if (constant.array_size > 0 || unknown_size) {
								bool full_def = false;

								VariableDeclarationNode::Declaration decl;
								decl.name = name;
								decl.size = constant.array_size;

								tk = _get_token();

								if (tk.type != TK_CURLY_BRACKET_OPEN) {
									if (unknown_size) {
										_set_expected_error("{");
										return ERR_PARSE_ERROR;
									}

									full_def = true;

									DataPrecision precision2 = PRECISION_DEFAULT;
									if (is_token_precision(tk.type)) {
										precision2 = get_token_precision(tk.type);
										tk = _get_token();
										if (!is_token_nonvoid_datatype(tk.type)) {
											_set_error(RTR("Expected data type after precision modifier."));
											return ERR_PARSE_ERROR;
										}
									}

									StringName struct_name2;
									DataType type2;

									if (shader->structs.has(tk.text)) {
										type2 = TYPE_STRUCT;
										struct_name2 = tk.text;
									} else {
										if (!is_token_variable_datatype(tk.type)) {
											_set_error(RTR("Invalid data type for the array."));
											return ERR_PARSE_ERROR;
										}
										type2 = get_token_datatype(tk.type);
									}

									int array_size2 = 0;
									tk = _get_token();

									if (tk.type == TK_BRACKET_OPEN) {
										bool is_unknown_size = false;
										Error error = _parse_array_size(nullptr, constants, false, nullptr, &array_size2, &is_unknown_size);
										if (error != OK) {
											return error;
										}
										if (is_unknown_size) {
											array_size2 = constant.array_size;
										}
										tk = _get_token();
									} else {
										_set_expected_error("[");
										return ERR_PARSE_ERROR;
									}

									if (constant.precision != precision2 || constant.type != type2 || struct_name != struct_name2 || constant.array_size != array_size2) {
										String from;
										if (type2 == TYPE_STRUCT) {
											from += struct_name2;
										} else {
											if (precision2 != PRECISION_DEFAULT) {
												from += get_precision_name(precision2);
												from += " ";
											}
											from += get_datatype_name(type2);
										}
										from += "[";
										from += itos(array_size2);
										from += "]'";

										String to;
										if (type == TYPE_STRUCT) {
											to += struct_name;
										} else {
											if (precision != PRECISION_DEFAULT) {
												to += get_precision_name(precision);
												to += " ";
											}
											to += get_datatype_name(type);
										}
										to += "[";
										to += itos(constant.array_size);
										to += "]'";

										_set_error(vformat(RTR("Cannot convert from '%s' to '%s'."), from, to));
										return ERR_PARSE_ERROR;
									}
								}

								bool curly = tk.type == TK_CURLY_BRACKET_OPEN;

								if (unknown_size) {
									if (!curly) {
										_set_expected_error("{");
										return ERR_PARSE_ERROR;
									}
								} else {
									if (full_def) {
										if (curly) {
											_set_expected_error("(");
											return ERR_PARSE_ERROR;
										}
									}
								}

								if (tk.type == TK_PARENTHESIS_OPEN || curly) { // initialization
									while (true) {
										Node *n = _parse_and_reduce_expression(nullptr, constants);
										if (!n) {
											return ERR_PARSE_ERROR;
										}

										if (n->type == Node::NODE_TYPE_OPERATOR && static_cast<OperatorNode *>(n)->op == OP_CALL) {
											_set_error(RTR("Expected constant expression."));
											return ERR_PARSE_ERROR;
										}

										if (!_compare_datatypes(constant.type, struct_name, 0, n->get_datatype(), n->get_datatype_name(), 0)) {
											return ERR_PARSE_ERROR;
										}

										tk = _get_token();
										if (tk.type == TK_COMMA) {
											decl.initializer.push_back(n);
											continue;
										} else if (!curly && tk.type == TK_PARENTHESIS_CLOSE) {
											decl.initializer.push_back(n);
											break;
										} else if (curly && tk.type == TK_CURLY_BRACKET_CLOSE) {
											decl.initializer.push_back(n);
											break;
										} else {
											if (curly) {
												_set_expected_error("}", ",");
											} else {
												_set_expected_error(")", ",");
											}
											return ERR_PARSE_ERROR;
										}
									}
									if (unknown_size) {
										decl.size = decl.initializer.size();
										constant.array_size = decl.initializer.size();
									} else if (decl.initializer.size() != constant.array_size) {
										_set_error(vformat(RTR("Array size mismatch. Expected %d elements (found %d)."), constant.array_size, decl.initializer.size()));
										return ERR_PARSE_ERROR;
									}
								} else {
									_set_expected_error("(");
									return ERR_PARSE_ERROR;
								}

								array_size = constant.array_size;

								ConstantNode *expr = memnew(ConstantNode);

								expr->datatype = constant.type;

								expr->struct_name = constant.struct_name;

								expr->array_size = constant.array_size;

								expr->array_declarations.push_back(decl);

								constant.initializer = static_cast<ConstantNode *>(expr);
							} else {
#ifdef DEBUG_ENABLED
								if (constant.type == DataType::TYPE_BOOL) {
									keyword_completion_context = CF_BOOLEAN;
								}
#endif // DEBUG_ENABLED

								//variable created with assignment! must parse an expression
								Node *expr = _parse_and_reduce_expression(nullptr, constants);
								if (!expr) {
									return ERR_PARSE_ERROR;
								}
#ifdef DEBUG_ENABLED
								if (constant.type == DataType::TYPE_BOOL) {
									keyword_completion_context = CF_GLOBAL_SPACE;
								}
#endif // DEBUG_ENABLED
								if (expr->type == Node::NODE_TYPE_OPERATOR && static_cast<OperatorNode *>(expr)->op == OP_CALL) {
									OperatorNode *op = static_cast<OperatorNode *>(expr);
									for (int i = 1; i < op->arguments.size(); i++) {
										if (!_check_node_constness(op->arguments[i])) {
											_set_error(vformat(RTR("Expected constant expression for argument %d of function call after '='."), i - 1));
											return ERR_PARSE_ERROR;
										}
									}
								}

								constant.initializer = expr;

								if (!_compare_datatypes(type, struct_name, 0, expr->get_datatype(), expr->get_datatype_name(), expr->get_array_size())) {
									return ERR_PARSE_ERROR;
								}
							}
							tk = _get_token();
						} else {
							if (constant.array_size > 0 || unknown_size) {
								_set_error(RTR("Expected array initialization."));
								return ERR_PARSE_ERROR;
							} else {
								_set_error(RTR("Expected initialization of constant."));
								return ERR_PARSE_ERROR;
							}
						}

						shader->constants[name] = constant;
						shader->vconstants.push_back(constant);
#ifdef DEBUG_ENABLED
						if (check_warnings && HAS_WARNING(ShaderWarning::UNUSED_CONSTANT_FLAG)) {
							used_constants.insert(name, Usage(tk_line));
						}
#endif // DEBUG_ENABLED

						if (tk.type == TK_COMMA) {
							tk = _get_token();
							if (tk.type != TK_IDENTIFIER) {
								_set_error(RTR("Expected an identifier after type."));
								return ERR_PARSE_ERROR;
							}

							name = tk.text;
							if (_find_identifier(nullptr, false, constants, name)) {
								_set_redefinition_error(String(name));
								return ERR_PARSE_ERROR;
							}

							if (has_builtin(p_functions, name)) {
								_set_redefinition_error(String(name));
								return ERR_PARSE_ERROR;
							}

							tk = _get_token();

							if (!fixed_array_size) {
								array_size = 0;
							}
							unknown_size = false;

						} else if (tk.type == TK_SEMICOLON) {
							is_const_decl = false;
							break;
						} else {
							_set_expected_error(",", ";");
							return ERR_PARSE_ERROR;
						}
					}

					break;
				}

				if (is_constant) {
					_set_error(vformat(RTR("'%s' qualifier cannot be used with a function return type."), "const"));
					return ERR_PARSE_ERROR;
				}

				FunctionInfo builtins;
				if (p_functions.has(name)) {
					builtins = p_functions[name];
				}

				if (p_functions.has("global")) { // Adds global variables: 'TIME'
					for (const KeyValue<StringName, BuiltInInfo> &E : p_functions["global"].built_ins) {
						builtins.built_ins.insert(E.key, E.value);
					}
				}

				if (p_functions.has("constants")) { // Adds global constants: 'PI', 'TAU', 'E'
					for (const KeyValue<StringName, BuiltInInfo> &E : p_functions["constants"].built_ins) {
						builtins.built_ins.insert(E.key, E.value);
					}
				}

				for (int i = 0; i < shader->vfunctions.size(); i++) {
					if (!shader->vfunctions[i].callable && shader->vfunctions[i].name == name) {
						_set_redefinition_error(String(name));
						return ERR_PARSE_ERROR;
					}
				}

				ShaderNode::Function function;

				function.callable = !p_functions.has(name);
				function.name = name;
				function.rname = name;

				FunctionNode *func_node = alloc_node<FunctionNode>();
				function.function = func_node;

				func_node->name = name;
				func_node->rname = name;
				func_node->return_type = type;
				func_node->return_struct_name = struct_name;
				func_node->return_precision = precision;
				func_node->return_array_size = array_size;

				if (p_functions.has(name)) {
					func_node->can_discard = p_functions[name].can_discard;
				} else {
					func_node->can_discard = is_discard_supported; // Allow use it for custom functions (in supported shader types).
				}

				if (!function_overload_count.has(name)) {
					function_overload_count.insert(name, 0);
				} else {
					function_overload_count[name]++;
				}

				func_node->body = alloc_node<BlockNode>();
				func_node->body->parent_function = func_node;

				tk = _get_token();

				while (true) {
					if (tk.type == TK_PARENTHESIS_CLOSE) {
						break;
					}
#ifdef DEBUG_ENABLED
					keyword_completion_context = CF_CONST_KEYWORD | CF_FUNC_DECL_PARAM_SPEC | CF_PRECISION_MODIFIER | CF_FUNC_DECL_PARAM_TYPE; // eg. const in mediump float

					if (_lookup_next(next)) {
						if (next.type == TK_CONST) {
							keyword_completion_context = CF_UNSPECIFIED;
						} else if (is_token_arg_qual(next.type)) {
							keyword_completion_context = CF_CONST_KEYWORD;
						} else if (is_token_precision(next.type)) {
							keyword_completion_context = (CF_CONST_KEYWORD | CF_FUNC_DECL_PARAM_SPEC | CF_FUNC_DECL_PARAM_TYPE);
						} else if (is_token_datatype(next.type)) {
							keyword_completion_context = (CF_CONST_KEYWORD | CF_FUNC_DECL_PARAM_SPEC | CF_PRECISION_MODIFIER);
						}
					}
#endif // DEBUG_ENABLED

					bool param_is_const = false;
					if (tk.type == TK_CONST) {
						param_is_const = true;
						tk = _get_token();
#ifdef DEBUG_ENABLED
						if (keyword_completion_context & CF_CONST_KEYWORD) {
							keyword_completion_context ^= CF_CONST_KEYWORD;
						}

						if (_lookup_next(next)) {
							if (is_token_arg_qual(next.type)) {
								keyword_completion_context = CF_UNSPECIFIED;
							} else if (is_token_precision(next.type)) {
								keyword_completion_context = (CF_FUNC_DECL_PARAM_SPEC | CF_FUNC_DECL_PARAM_TYPE);
							} else if (is_token_datatype(next.type)) {
								keyword_completion_context = (CF_FUNC_DECL_PARAM_SPEC | CF_PRECISION_MODIFIER);
							}
						}
#endif // DEBUG_ENABLED
					}

					ArgumentQualifier param_qualifier = ARGUMENT_QUALIFIER_IN;
					if (is_token_arg_qual(tk.type)) {
						bool error = false;
						switch (tk.type) {
							case TK_ARG_IN: {
								param_qualifier = ARGUMENT_QUALIFIER_IN;
							} break;
							case TK_ARG_OUT: {
								if (param_is_const) {
									_set_error(vformat(RTR("The '%s' qualifier cannot be used within a function parameter declared with '%s'."), "out", "const"));
									error = true;
								}
								param_qualifier = ARGUMENT_QUALIFIER_OUT;
							} break;
							case TK_ARG_INOUT: {
								if (param_is_const) {
									_set_error(vformat(RTR("The '%s' qualifier cannot be used within a function parameter declared with '%s'."), "inout", "const"));
									error = true;
								}
								param_qualifier = ARGUMENT_QUALIFIER_INOUT;
							} break;
							default:
								error = true;
								break;
						}
						tk = _get_token();
#ifdef DEBUG_ENABLED
						if (keyword_completion_context & CF_CONST_KEYWORD) {
							keyword_completion_context ^= CF_CONST_KEYWORD;
						}
						if (keyword_completion_context & CF_FUNC_DECL_PARAM_SPEC) {
							keyword_completion_context ^= CF_FUNC_DECL_PARAM_SPEC;
						}

						if (_lookup_next(next)) {
							if (is_token_precision(next.type)) {
								keyword_completion_context = CF_FUNC_DECL_PARAM_TYPE;
							} else if (is_token_datatype(next.type)) {
								keyword_completion_context = CF_PRECISION_MODIFIER;
							}
						}
#endif // DEBUG_ENABLED
						if (error) {
							return ERR_PARSE_ERROR;
						}
					}

					DataType param_type;
					StringName param_name;
					StringName param_struct_name;
					DataPrecision param_precision = PRECISION_DEFAULT;
					int arg_array_size = 0;

					if (is_token_precision(tk.type)) {
						param_precision = get_token_precision(tk.type);
						tk = _get_token();
#ifdef DEBUG_ENABLED
						if (keyword_completion_context & CF_CONST_KEYWORD) {
							keyword_completion_context ^= CF_CONST_KEYWORD;
						}
						if (keyword_completion_context & CF_FUNC_DECL_PARAM_SPEC) {
							keyword_completion_context ^= CF_FUNC_DECL_PARAM_SPEC;
						}
						if (keyword_completion_context & CF_PRECISION_MODIFIER) {
							keyword_completion_context ^= CF_PRECISION_MODIFIER;
						}

						if (_lookup_next(next)) {
							if (is_token_datatype(next.type)) {
								keyword_completion_context = CF_UNSPECIFIED;
							}
						}
#endif // DEBUG_ENABLED
					}

					is_struct = false;

					if (shader->structs.has(tk.text)) {
						is_struct = true;
						param_struct_name = tk.text;
#ifdef DEBUG_ENABLED
						if (check_warnings && HAS_WARNING(ShaderWarning::UNUSED_STRUCT_FLAG) && used_structs.has(param_struct_name)) {
							used_structs[param_struct_name].used = true;
						}
#endif // DEBUG_ENABLED
					}

					if (!is_struct && !is_token_datatype(tk.type)) {
						_set_error(RTR("Expected a valid data type for argument."));
						return ERR_PARSE_ERROR;
					}

					if (param_qualifier == ARGUMENT_QUALIFIER_OUT || param_qualifier == ARGUMENT_QUALIFIER_INOUT) {
						if (is_sampler_type(get_token_datatype(tk.type))) {
							_set_error(RTR("Opaque types cannot be output parameters."));
							return ERR_PARSE_ERROR;
						}
					}

					if (is_struct) {
						param_type = TYPE_STRUCT;
					} else {
						param_type = get_token_datatype(tk.type);
						if (param_type == TYPE_VOID) {
							_set_error(RTR("Void type not allowed as argument."));
							return ERR_PARSE_ERROR;
						}
					}

					if (param_precision != PRECISION_DEFAULT && _validate_precision(param_type, param_precision) != OK) {
						return ERR_PARSE_ERROR;
					}
#ifdef DEBUG_ENABLED
					keyword_completion_context = CF_UNSPECIFIED;
#endif // DEBUG_ENABLED
					tk = _get_token();

					if (tk.type == TK_BRACKET_OPEN) {
						Error error = _parse_array_size(nullptr, constants, true, nullptr, &arg_array_size, nullptr);
						if (error != OK) {
							return error;
						}
						tk = _get_token();
					}
					if (tk.type != TK_IDENTIFIER) {
						_set_error(RTR("Expected an identifier for argument name."));
						return ERR_PARSE_ERROR;
					}

					param_name = tk.text;

					if (_find_identifier(func_node->body, false, builtins, param_name, (ShaderLanguage::DataType *)nullptr, &itype)) {
						if (itype != IDENTIFIER_FUNCTION) {
							_set_redefinition_error(String(param_name));
							return ERR_PARSE_ERROR;
						}
					}

					if (has_builtin(p_functions, param_name)) {
						_set_redefinition_error(String(param_name));
						return ERR_PARSE_ERROR;
					}

					FunctionNode::Argument arg;
					arg.type = param_type;
					arg.name = param_name;
					arg.struct_name = param_struct_name;
					arg.precision = param_precision;
					arg.qualifier = param_qualifier;
					arg.tex_argument_check = false;
					arg.tex_builtin_check = false;
					arg.tex_argument_filter = FILTER_DEFAULT;
					arg.tex_argument_repeat = REPEAT_DEFAULT;
					arg.is_const = param_is_const;

					tk = _get_token();
					if (tk.type == TK_BRACKET_OPEN) {
						Error error = _parse_array_size(nullptr, constants, true, nullptr, &arg_array_size, nullptr);
						if (error != OK) {
							return error;
						}
						tk = _get_token();
					}

					arg.array_size = arg_array_size;
					func_node->arguments.push_back(arg);

					if (tk.type == TK_COMMA) {
						tk = _get_token();
						//do none and go on
					} else if (tk.type != TK_PARENTHESIS_CLOSE) {
						_set_expected_error(",", ")");
						return ERR_PARSE_ERROR;
					}
				}

				// Searches for function index and check for the exact duplicate in overloads.
				int function_index = 0;
				for (int i = 0; i < shader->vfunctions.size(); i++) {
					if (!shader->vfunctions[i].callable || shader->vfunctions[i].rname != name) {
						continue;
					}

					function_index++;

					if (shader->vfunctions[i].function->arguments.size() != func_node->arguments.size()) {
						continue;
					}

					bool is_same = true;

					for (int j = 0; j < shader->vfunctions[i].function->arguments.size(); j++) {
						FunctionNode::Argument a = func_node->arguments[j];
						FunctionNode::Argument b = shader->vfunctions[i].function->arguments[j];

						if (a.type == b.type && a.array_size == b.array_size) {
							if (a.type == TYPE_STRUCT) {
								is_same = a.struct_name == b.struct_name;
							}
						} else {
							is_same = false;
						}

						if (!is_same) {
							break;
						}
					}

					if (is_same) {
						_set_redefinition_error(String(name));
						return ERR_PARSE_ERROR;
					}
				}

				// Creates a fake name for function overload, which will be replaced by the real name by the compiler.
				String name2 = name;
				if (function_index > 0) {
					name2 = vformat("%s@%s", name, itos(function_index + 1));

					function.name = name2;
					func_node->name = name2;
				}

				shader->functions.insert(name2, function);
				shader->vfunctions.push_back(function);

				CallInfo call_info;
				call_info.name = name2;
				calls_info.insert(name2, call_info);

#ifdef DEBUG_ENABLED
				if (check_warnings && HAS_WARNING(ShaderWarning::UNUSED_FUNCTION_FLAG) && !p_functions.has(name)) {
					used_functions.insert(name2, Usage(tk_line));
				}
#endif // DEBUG_ENABLED

				if (p_functions.has(name)) {
					//if one of the core functions, make sure they are of the correct form
					if (func_node->arguments.size() > 0) {
						_set_error(vformat(RTR("Function '%s' expects no arguments."), String(name)));
						return ERR_PARSE_ERROR;
					}
					if (func_node->return_type != TYPE_VOID) {
						_set_error(vformat(RTR("Function '%s' must be of '%s' return type."), String(name), "void"));
						return ERR_PARSE_ERROR;
					}
				}

				//all good let's parse inside the function!
				tk = _get_token();
				if (tk.type != TK_CURLY_BRACKET_OPEN) {
					_set_error(RTR("Expected a '{' to begin function."));
					return ERR_PARSE_ERROR;
				}

				current_function = name2;

#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_BLOCK;
#endif // DEBUG_ENABLED
				Error err = _parse_block(func_node->body, builtins);
				if (err) {
					return err;
				}
#ifdef DEBUG_ENABLED
				keyword_completion_context = CF_GLOBAL_SPACE;
#endif // DEBUG_ENABLED
				if (func_node->return_type != DataType::TYPE_VOID) {
					BlockNode *block = func_node->body;
					if (_find_last_flow_op_in_block(block, FlowOperation::FLOW_OP_RETURN) != OK) {
						_set_error(vformat(RTR("Expected at least one '%s' statement in a non-void function."), "return"));
						return ERR_PARSE_ERROR;
					}
				}
				current_function = StringName();
			}
		}

		tk = _get_token();
	}

#ifdef DEBUG_ENABLED
	if (check_device_limit_warnings && uniform_buffer_exceeded_line != -1) {
		_add_warning(ShaderWarning::DEVICE_LIMIT_EXCEEDED, uniform_buffer_exceeded_line, RTR("uniform buffer"), { uniform_buffer_size, max_uniform_buffer_size });
	}
#endif // DEBUG_ENABLED
	return OK;
}

bool ShaderLanguage::has_builtin(const HashMap<StringName, ShaderLanguage::FunctionInfo> &p_functions, const StringName &p_name, bool p_check_global_funcs) {
	if (p_check_global_funcs && global_func_set.has(p_name)) {
		return true;
	}

	for (const KeyValue<StringName, ShaderLanguage::FunctionInfo> &E : p_functions) {
		if (E.value.built_ins.has(p_name)) {
			return true;
		}
	}

	return false;
}

Error ShaderLanguage::_find_last_flow_op_in_op(ControlFlowNode *p_flow, FlowOperation p_op) {
	bool found = false;

	for (int i = p_flow->blocks.size() - 1; i >= 0; i--) {
		if (p_flow->blocks[i]->type == Node::NODE_TYPE_BLOCK) {
			BlockNode *last_block = static_cast<BlockNode *>(p_flow->blocks[i]);
			if (_find_last_flow_op_in_block(last_block, p_op) == OK) {
				found = true;
				break;
			}
		}
	}
	if (found) {
		return OK;
	}
	return FAILED;
}

Error ShaderLanguage::_find_last_flow_op_in_block(BlockNode *p_block, FlowOperation p_op) {
	bool found = false;

	for (List<ShaderLanguage::Node *>::Element *E = p_block->statements.back(); E; E = E->prev()) {
		if (E->get()->type == Node::NODE_TYPE_CONTROL_FLOW) {
			ControlFlowNode *flow = static_cast<ControlFlowNode *>(E->get());
			if (flow->flow_op == p_op) {
				found = true;
				break;
			} else {
				if (_find_last_flow_op_in_op(flow, p_op) == OK) {
					found = true;
					break;
				}
			}
		} else if (E->get()->type == Node::NODE_TYPE_BLOCK) {
			BlockNode *block = static_cast<BlockNode *>(E->get());
			if (_find_last_flow_op_in_block(block, p_op) == OK) {
				found = true;
				break;
			}
		}
	}

	if (found) {
		return OK;
	}
	return FAILED;
}

// skips over whitespace and /* */ and // comments
static int _get_first_ident_pos(const String &p_code) {
	int idx = 0;

#define GETCHAR(m_idx) (((idx + m_idx) < p_code.length()) ? p_code[idx + m_idx] : char32_t(0))

	while (true) {
		if (GETCHAR(0) == '/' && GETCHAR(1) == '/') {
			idx += 2;
			while (true) {
				if (GETCHAR(0) == 0) {
					return 0;
				}
				if (GETCHAR(0) == '\n') {
					idx++;
					break; // loop
				}
				idx++;
			}
		} else if (GETCHAR(0) == '/' && GETCHAR(1) == '*') {
			idx += 2;
			while (true) {
				if (GETCHAR(0) == 0) {
					return 0;
				}
				if (GETCHAR(0) == '*' && GETCHAR(1) == '/') {
					idx += 2;
					break; // loop
				}
				idx++;
			}
		} else {
			switch (GETCHAR(0)) {
				case ' ':
				case '\t':
				case '\r':
				case '\n': {
					idx++;
				} break; // switch
				default:
					return idx;
			}
		}
	}

#undef GETCHAR
}

String ShaderLanguage::get_shader_type(const String &p_code) {
	bool reading_type = false;

	String cur_identifier;

	for (int i = _get_first_ident_pos(p_code); i < p_code.length(); i++) {
		if (p_code[i] == ';') {
			break;

		} else if (p_code[i] <= 32) {
			if (!cur_identifier.is_empty()) {
				if (!reading_type) {
					if (cur_identifier != "shader_type") {
						return String();
					}

					reading_type = true;
					cur_identifier = String();
				} else {
					return cur_identifier;
				}
			}
		} else {
			cur_identifier += String::chr(p_code[i]);
		}
	}

	if (reading_type) {
		return cur_identifier;
	}

	return String();
}

bool ShaderLanguage::is_builtin_func_out_parameter(const String &p_name, int p_param) {
	int i = 0;
	while (builtin_func_out_args[i].name) {
		if (p_name == builtin_func_out_args[i].name) {
			for (int j = 0; j < BuiltinFuncOutArgs::MAX_ARGS; j++) {
				int arg = builtin_func_out_args[i].arguments[j];
				if (arg == p_param) {
					return true;
				}
				if (arg < 0) {
					return false;
				}
			}
		}
		i++;
	}
	return false;
}

#ifdef DEBUG_ENABLED
void ShaderLanguage::_check_warning_accums() {
	for (const KeyValue<ShaderWarning::Code, HashMap<StringName, HashMap<StringName, Usage>> *> &E : warnings_check_map2) {
		for (const KeyValue<StringName, HashMap<StringName, Usage>> &T : *E.value) {
			for (const KeyValue<StringName, Usage> &U : T.value) {
				if (!U.value.used) {
					_add_warning(E.key, U.value.decl_line, U.key);
				}
			}
		}
	}
	for (const KeyValue<ShaderWarning::Code, HashMap<StringName, Usage> *> &E : warnings_check_map) {
		for (const KeyValue<StringName, Usage> &U : (*E.value)) {
			if (!U.value.used) {
				_add_warning(E.key, U.value.decl_line, U.key);
			}
		}
	}
}
List<ShaderWarning>::Element *ShaderLanguage::get_warnings_ptr() {
	return warnings.front();
}
void ShaderLanguage::enable_warning_checking(bool p_enabled) {
	check_warnings = p_enabled;
}
bool ShaderLanguage::is_warning_checking_enabled() const {
	return check_warnings;
}
void ShaderLanguage::set_warning_flags(uint32_t p_flags) {
	warning_flags = p_flags;
}
uint32_t ShaderLanguage::get_warning_flags() const {
	return warning_flags;
}
#endif // DEBUG_ENABLED

Error ShaderLanguage::compile(const String &p_code, const ShaderCompileInfo &p_info) {
	clear();
	is_shader_inc = p_info.is_include;

	code = p_code;
	global_shader_uniform_get_type_func = p_info.global_shader_uniform_type_func;

	varying_function_names = p_info.varying_function_names;

	nodes = nullptr;

	shader = alloc_node<ShaderNode>();
	Error err = _parse_shader(p_info.functions, p_info.render_modes, p_info.shader_types);

#ifdef DEBUG_ENABLED
	if (check_warnings) {
		_check_warning_accums();
	}
#endif // DEBUG_ENABLED

	if (err != OK) {
		return err;
	}
	return OK;
}

Error ShaderLanguage::complete(const String &p_code, const ShaderCompileInfo &p_info, List<ScriptLanguage::CodeCompletionOption> *r_options, String &r_call_hint) {
	clear();
	is_shader_inc = p_info.is_include;

	code = p_code;
	varying_function_names = p_info.varying_function_names;

	nodes = nullptr;
	global_shader_uniform_get_type_func = p_info.global_shader_uniform_type_func;

	shader = alloc_node<ShaderNode>();
	_parse_shader(p_info.functions, p_info.render_modes, p_info.shader_types);

#ifdef DEBUG_ENABLED
	// Adds context keywords.
	if (keyword_completion_context != CF_UNSPECIFIED) {
		int sz = sizeof(keyword_list) / sizeof(KeyWord);
		for (int i = 0; i < sz; i++) {
			if (keyword_list[i].flags == CF_UNSPECIFIED) {
				break; // Ignore hint keywords (parsed below).
			}
			if (keyword_list[i].flags & keyword_completion_context) {
				if (keyword_list[i].excluded_shader_types.has(shader_type_identifier) || keyword_list[i].excluded_functions.has(current_function)) {
					continue;
				}
				ScriptLanguage::CodeCompletionOption option(keyword_list[i].text, ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
				r_options->push_back(option);
			}
		}
	}
#endif // DEBUG_ENABLED

	switch (completion_type) {
		case COMPLETION_NONE: {
			//do nothing
			return OK;
		} break;
		case COMPLETION_SHADER_TYPE: {
			for (const String &shader_type : p_info.shader_types) {
				ScriptLanguage::CodeCompletionOption option(shader_type, ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
				r_options->push_back(option);
			}
			return OK;
		} break;
		case COMPLETION_RENDER_MODE: {
			if (is_shader_inc) {
				for (int i = 0; i < RenderingServer::SHADER_MAX; i++) {
					const Vector<ModeInfo> modes = ShaderTypes::get_singleton()->get_modes(RenderingServer::ShaderMode(i));

					for (int j = 0; j < modes.size(); j++) {
						const ModeInfo &info = modes[j];

						if (!info.options.is_empty()) {
							bool found = false;

							for (int k = 0; k < info.options.size(); k++) {
								if (shader->render_modes.has(String(info.name) + "_" + String(info.options[k]))) {
									found = true;
								}
							}

							if (!found) {
								for (int k = 0; k < info.options.size(); k++) {
									ScriptLanguage::CodeCompletionOption option(String(info.name) + "_" + String(info.options[k]), ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
									r_options->push_back(option);
								}
							}
						} else {
							const String name = String(info.name);

							if (!shader->render_modes.has(name)) {
								ScriptLanguage::CodeCompletionOption option(name, ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
								r_options->push_back(option);
							}
						}
					}
				}
			} else {
				for (int i = 0; i < p_info.render_modes.size(); i++) {
					const ModeInfo &info = p_info.render_modes[i];

					if (!info.options.is_empty()) {
						bool found = false;

						for (int j = 0; j < info.options.size(); j++) {
							if (shader->render_modes.has(String(info.name) + "_" + String(info.options[j]))) {
								found = true;
							}
						}

						if (!found) {
							for (int j = 0; j < info.options.size(); j++) {
								ScriptLanguage::CodeCompletionOption option(String(info.name) + "_" + String(info.options[j]), ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
								r_options->push_back(option);
							}
						}
					} else {
						const String name = String(info.name);

						if (!shader->render_modes.has(name)) {
							ScriptLanguage::CodeCompletionOption option(name, ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
							r_options->push_back(option);
						}
					}
				}
			}

			return OK;
		} break;
		case COMPLETION_STRUCT: {
			if (shader->structs.has(completion_struct)) {
				StructNode *node = shader->structs[completion_struct].shader_struct;
				for (ShaderLanguage::MemberNode *member : node->members) {
					ScriptLanguage::CodeCompletionOption option(member->name, ScriptLanguage::CODE_COMPLETION_KIND_MEMBER);
					r_options->push_back(option);
				}
			}

			return OK;
		} break;
		case COMPLETION_MAIN_FUNCTION: {
			for (const KeyValue<StringName, FunctionInfo> &E : p_info.functions) {
				if (!E.value.main_function) {
					continue;
				}
				bool found = false;
				for (int i = 0; i < shader->vfunctions.size(); i++) {
					if (shader->vfunctions[i].name == E.key) {
						found = true;
						break;
					}
				}
				if (found) {
					continue;
				}
				ScriptLanguage::CodeCompletionOption option(E.key, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
				r_options->push_back(option);
			}

			return OK;
		} break;
		case COMPLETION_IDENTIFIER:
		case COMPLETION_FUNCTION_CALL: {
			bool comp_ident = completion_type == COMPLETION_IDENTIFIER;
			HashMap<String, ScriptLanguage::CodeCompletionKind> matches;
			StringName skip_function;
			BlockNode *block = completion_block;

			if (completion_class == TAG_GLOBAL) {
				while (block) {
					if (comp_ident) {
						for (const KeyValue<StringName, BlockNode::Variable> &E : block->variables) {
							if (E.value.line < completion_line) {
								matches.insert(E.key, ScriptLanguage::CODE_COMPLETION_KIND_VARIABLE);
							}
						}
					}

					if (block->parent_function) {
						if (comp_ident) {
							for (int i = 0; i < block->parent_function->arguments.size(); i++) {
								matches.insert(block->parent_function->arguments[i].name, ScriptLanguage::CODE_COMPLETION_KIND_VARIABLE);
							}
						}
						skip_function = block->parent_function->name;
					}
					block = block->parent_block;
				}

				if (comp_ident) {
					if (is_shader_inc) {
						for (int i = 0; i < RenderingServer::SHADER_MAX; i++) {
							const HashMap<StringName, ShaderLanguage::FunctionInfo> info = ShaderTypes::get_singleton()->get_functions(RenderingServer::ShaderMode(i));

							if (info.has("global")) {
								for (const KeyValue<StringName, BuiltInInfo> &E : info["global"].built_ins) {
									ScriptLanguage::CodeCompletionKind kind = ScriptLanguage::CODE_COMPLETION_KIND_MEMBER;
									if (E.value.constant) {
										kind = ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT;
									}
									matches.insert(E.key, kind);
								}
							}

							if (info.has("constants")) {
								for (const KeyValue<StringName, BuiltInInfo> &E : info["constants"].built_ins) {
									ScriptLanguage::CodeCompletionKind kind = ScriptLanguage::CODE_COMPLETION_KIND_MEMBER;
									if (E.value.constant) {
										kind = ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT;
									}
									matches.insert(E.key, kind);
								}
							}

							if (skip_function != StringName() && info.has(skip_function)) {
								for (const KeyValue<StringName, BuiltInInfo> &E : info[skip_function].built_ins) {
									ScriptLanguage::CodeCompletionKind kind = ScriptLanguage::CODE_COMPLETION_KIND_MEMBER;
									if (E.value.constant) {
										kind = ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT;
									}
									matches.insert(E.key, kind);
								}
							}
						}
					} else {
						if (p_info.functions.has("global")) {
							for (const KeyValue<StringName, BuiltInInfo> &E : p_info.functions["global"].built_ins) {
								ScriptLanguage::CodeCompletionKind kind = ScriptLanguage::CODE_COMPLETION_KIND_MEMBER;
								if (E.value.constant) {
									kind = ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT;
								}
								matches.insert(E.key, kind);
							}
						}

						if (p_info.functions.has("constants")) {
							for (const KeyValue<StringName, BuiltInInfo> &E : p_info.functions["constants"].built_ins) {
								ScriptLanguage::CodeCompletionKind kind = ScriptLanguage::CODE_COMPLETION_KIND_MEMBER;
								if (E.value.constant) {
									kind = ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT;
								}
								matches.insert(E.key, kind);
							}
						}

						if (skip_function != StringName() && p_info.functions.has(skip_function)) {
							for (const KeyValue<StringName, BuiltInInfo> &E : p_info.functions[skip_function].built_ins) {
								ScriptLanguage::CodeCompletionKind kind = ScriptLanguage::CODE_COMPLETION_KIND_MEMBER;
								if (E.value.constant) {
									kind = ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT;
								}
								matches.insert(E.key, kind);
							}
						}
					}

					for (const KeyValue<StringName, ShaderNode::Constant> &E : shader->constants) {
						matches.insert(E.key, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT);
					}
					for (const KeyValue<StringName, ShaderNode::Varying> &E : shader->varyings) {
						matches.insert(E.key, ScriptLanguage::CODE_COMPLETION_KIND_VARIABLE);
					}
					for (const KeyValue<StringName, ShaderNode::Uniform> &E : shader->uniforms) {
						matches.insert(E.key, ScriptLanguage::CODE_COMPLETION_KIND_MEMBER);
					}
				}

				for (int i = 0; i < shader->vfunctions.size(); i++) {
					if (!shader->vfunctions[i].callable || shader->vfunctions[i].name == skip_function) {
						continue;
					}
					matches.insert(String(shader->vfunctions[i].rname), ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
				}

				int idx = 0;
				bool low_end = RenderingServer::get_singleton()->is_low_end();

				if (stages) {
					// Stage functions can be used in custom functions as well, that why need to check them all.
					for (const KeyValue<StringName, FunctionInfo> &E : *stages) {
						for (const KeyValue<StringName, StageFunctionInfo> &F : E.value.stage_functions) {
							if (F.value.skip_function == skip_function && stages->has(skip_function)) {
								continue;
							}
							matches.insert(String(F.key), ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
						}
					}
				}

				while (builtin_func_defs[idx].name) {
					if ((low_end && builtin_func_defs[idx].high_end) || _check_restricted_func(builtin_func_defs[idx].name, skip_function)) {
						idx++;
						continue;
					}

					matches.insert(String(builtin_func_defs[idx].name), ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
					idx++;
				}

			} else { // sub-class
				int idx = 0;
				bool low_end = RenderingServer::get_singleton()->is_low_end();

				while (builtin_func_defs[idx].name) {
					if (low_end && builtin_func_defs[idx].high_end) {
						idx++;
						continue;
					}
					if (builtin_func_defs[idx].tag == completion_class) {
						matches.insert(String(builtin_func_defs[idx].name), ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
					}
					idx++;
				}
			}

			for (const KeyValue<String, ScriptLanguage::CodeCompletionKind> &E : matches) {
				ScriptLanguage::CodeCompletionOption option(E.key, E.value);
				if (E.value == ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION) {
					option.insert_text += "(";
				}
				r_options->push_back(option);
			}

			return OK;
		} break;
		case COMPLETION_CALL_ARGUMENTS: {
			StringName block_function;
			BlockNode *block = completion_block;
			String calltip;

			while (block) {
				if (block->parent_function) {
					block_function = block->parent_function->name;
				}
				block = block->parent_block;
			}

			for (int i = 0, overload_index = 0; i < shader->vfunctions.size(); i++) {
				if (!shader->vfunctions[i].callable || shader->vfunctions[i].rname != completion_function) {
					continue;
				}

				if (shader->vfunctions[i].function->return_type == TYPE_STRUCT) {
					calltip += String(shader->vfunctions[i].function->return_struct_name);
				} else {
					calltip += get_datatype_name(shader->vfunctions[i].function->return_type);
				}

				if (shader->vfunctions[i].function->return_array_size > 0) {
					calltip += "[";
					calltip += itos(shader->vfunctions[i].function->return_array_size);
					calltip += "]";
				}

				calltip += " ";
				calltip += shader->vfunctions[i].rname;
				calltip += "(";

				for (int j = 0; j < shader->vfunctions[i].function->arguments.size(); j++) {
					if (j > 0) {
						calltip += ", ";
					} else {
						calltip += " ";
					}

					if (j == completion_argument) {
						calltip += char32_t(0xFFFF);
					}

					if (shader->vfunctions[i].function->arguments[j].is_const) {
						calltip += "const ";
					}

					if (shader->vfunctions[i].function->arguments[j].qualifier != ArgumentQualifier::ARGUMENT_QUALIFIER_IN) {
						if (shader->vfunctions[i].function->arguments[j].qualifier == ArgumentQualifier::ARGUMENT_QUALIFIER_OUT) {
							calltip += "out ";
						} else { // ArgumentQualifier::ARGUMENT_QUALIFIER_INOUT
							calltip += "inout ";
						}
					}

					if (shader->vfunctions[i].function->arguments[j].type == TYPE_STRUCT) {
						calltip += String(shader->vfunctions[i].function->arguments[j].struct_name);
					} else {
						calltip += get_datatype_name(shader->vfunctions[i].function->arguments[j].type);
					}
					calltip += " ";
					calltip += shader->vfunctions[i].function->arguments[j].name;

					if (shader->vfunctions[i].function->arguments[j].array_size > 0) {
						calltip += "[";
						calltip += itos(shader->vfunctions[i].function->arguments[j].array_size);
						calltip += "]";
					}

					if (j == completion_argument) {
						calltip += char32_t(0xFFFF);
					}
				}

				if (shader->vfunctions[i].function->arguments.size()) {
					calltip += " ";
				}
				calltip += ")";

				if (overload_index < function_overload_count[shader->vfunctions[i].rname]) {
					overload_index++;
					calltip += "\n";
					continue;
				}

				r_call_hint = calltip;
				return OK;
			}

			if (stages) {
				// Stage functions can be used in custom functions as well, that why need to check them all.
				for (const KeyValue<StringName, FunctionInfo> &S : *stages) {
					for (const KeyValue<StringName, StageFunctionInfo> &E : S.value.stage_functions) {
						// No need to check for the skip function here.
						if (completion_function != E.key) {
							continue;
						}

						calltip += get_datatype_name(E.value.return_type);
						calltip += " ";
						calltip += E.key;
						calltip += "(";

						for (int i = 0; i < E.value.arguments.size(); i++) {
							if (i > 0) {
								calltip += ", ";
							} else {
								calltip += " ";
							}

							if (i == completion_argument) {
								calltip += char32_t(0xFFFF);
							}

							calltip += get_datatype_name(E.value.arguments[i].type);
							calltip += " ";
							calltip += E.value.arguments[i].name;

							if (i == completion_argument) {
								calltip += char32_t(0xFFFF);
							}
						}

						if (E.value.arguments.size()) {
							calltip += " ";
						}
						calltip += ")";

						r_call_hint = calltip;
						return OK;
					}
				}
			}

			int idx = 0;
			bool low_end = RenderingServer::get_singleton()->is_low_end();

			while (builtin_func_defs[idx].name) {
				if ((low_end && builtin_func_defs[idx].high_end) || _check_restricted_func(builtin_func_defs[idx].name, block_function)) {
					idx++;
					continue;
				}

				int idx2 = 0;
				HashSet<int> out_args;
				while (builtin_func_out_args[idx2].name != nullptr) {
					if (builtin_func_out_args[idx2].name == builtin_func_defs[idx].name) {
						for (int i = 0; i < BuiltinFuncOutArgs::MAX_ARGS; i++) {
							int arg = builtin_func_out_args[idx2].arguments[i];
							if (arg == -1) {
								break;
							}
							out_args.insert(arg);
						}
						break;
					}
					idx2++;
				}

				if (completion_function == builtin_func_defs[idx].name) {
					if (builtin_func_defs[idx].tag != completion_class) {
						idx++;
						continue;
					}

					if (calltip.length()) {
						calltip += "\n";
					}

					calltip += get_datatype_name(builtin_func_defs[idx].rettype);
					calltip += " ";
					calltip += builtin_func_defs[idx].name;
					calltip += "(";

					bool found_arg = false;
					for (int i = 0; i < BuiltinFuncDef::MAX_ARGS - 1; i++) {
						if (builtin_func_defs[idx].args[i] == TYPE_VOID) {
							break;
						}

						if (i > 0) {
							calltip += ", ";
						} else {
							calltip += " ";
						}

						if (i == completion_argument) {
							calltip += char32_t(0xFFFF);
						}

						if (out_args.has(i)) {
							calltip += "out ";
						}

						calltip += get_datatype_name(builtin_func_defs[idx].args[i]);

						String arg_name = (String)builtin_func_defs[idx].args_names[i];
						if (!arg_name.is_empty()) {
							calltip += " ";
							calltip += arg_name;
						}

						if (i == completion_argument) {
							calltip += char32_t(0xFFFF);
						}

						found_arg = true;
					}

					if (found_arg) {
						calltip += " ";
					}
					calltip += ")";
				}
				idx++;
			}

			r_call_hint = calltip;

			return OK;

		} break;
		case COMPLETION_INDEX: {
			const char colv[4] = { 'r', 'g', 'b', 'a' };
			const char coordv[4] = { 'x', 'y', 'z', 'w' };
			const char coordt[4] = { 's', 't', 'p', 'q' };
			const String theme_color_names[4] = { "axis_x_color", "axis_y_color", "axis_z_color", "axis_w_color" };

			int limit = 0;

			switch (completion_base) {
				case TYPE_BVEC2:
				case TYPE_IVEC2:
				case TYPE_UVEC2:
				case TYPE_VEC2: {
					limit = 2;

				} break;
				case TYPE_BVEC3:
				case TYPE_IVEC3:
				case TYPE_UVEC3:
				case TYPE_VEC3: {
					limit = 3;

				} break;
				case TYPE_BVEC4:
				case TYPE_IVEC4:
				case TYPE_UVEC4:
				case TYPE_VEC4: {
					limit = 4;

				} break;
				default: {
				}
			}

			for (int i = 0; i < limit; i++) {
				r_options->push_back(ScriptLanguage::CodeCompletionOption(String::chr(colv[i]), ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT, ScriptLanguage::LOCATION_OTHER, theme_color_names[i]));
				r_options->push_back(ScriptLanguage::CodeCompletionOption(String::chr(coordv[i]), ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT, ScriptLanguage::LOCATION_OTHER, theme_color_names[i]));
				r_options->push_back(ScriptLanguage::CodeCompletionOption(String::chr(coordt[i]), ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT, ScriptLanguage::LOCATION_OTHER, theme_color_names[i]));
			}

		} break;
		case COMPLETION_HINT: {
			if (completion_base == DataType::TYPE_VEC3 || completion_base == DataType::TYPE_VEC4) {
				if (current_uniform_hint == ShaderNode::Uniform::HINT_NONE) {
					ScriptLanguage::CodeCompletionOption option("source_color", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
					r_options->push_back(option);
				}
			} else if ((completion_base == DataType::TYPE_INT || completion_base == DataType::TYPE_FLOAT) && !completion_base_array) {
				if (current_uniform_hint == ShaderNode::Uniform::HINT_NONE) {
					Vector<String> options;

					if (completion_base == DataType::TYPE_INT) {
						options.push_back("hint_range(0, 100, 1)");
						options.push_back("hint_enum(\"Zero\", \"One\", \"Two\")");
					} else {
						options.push_back("hint_range(0.0, 1.0, 0.1)");
					}

					for (const String &option_text : options) {
						String hint_name = option_text.substr(0, option_text.find_char(char32_t('(')));
						ScriptLanguage::CodeCompletionOption option(hint_name, ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
						option.insert_text = option_text;
						r_options->push_back(option);
					}
				}
			} else if ((int(completion_base) > int(TYPE_MAT4) && int(completion_base) < int(TYPE_STRUCT))) {
				Vector<String> options;
				if (current_uniform_filter == FILTER_DEFAULT) {
					options.push_back("filter_linear");
					options.push_back("filter_linear_mipmap");
					options.push_back("filter_linear_mipmap_anisotropic");
					options.push_back("filter_nearest");
					options.push_back("filter_nearest_mipmap");
					options.push_back("filter_nearest_mipmap_anisotropic");
				}
				if (current_uniform_repeat == REPEAT_DEFAULT) {
					options.push_back("repeat_enable");
					options.push_back("repeat_disable");
				}
				if (completion_base_array) {
					if (current_uniform_hint == ShaderNode::Uniform::HINT_NONE) {
						options.push_back("source_color");
					}
				} else {
					if (current_uniform_hint == ShaderNode::Uniform::HINT_NONE) {
						options.push_back("hint_anisotropy");
						options.push_back("hint_default_black");
						options.push_back("hint_default_white");
						options.push_back("hint_default_transparent");
						options.push_back("hint_normal");
						options.push_back("hint_roughness_a");
						options.push_back("hint_roughness_b");
						options.push_back("hint_roughness_g");
						options.push_back("hint_roughness_gray");
						options.push_back("hint_roughness_normal");
						options.push_back("hint_roughness_r");
						options.push_back("hint_screen_texture");
						options.push_back("hint_normal_roughness_texture");
						options.push_back("hint_depth_texture");
						options.push_back("source_color");
					}
				}

				for (int i = 0; i < options.size(); i++) {
					ScriptLanguage::CodeCompletionOption option(options[i], ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
					r_options->push_back(option);
				}
			}
			if (!completion_base_array && !current_uniform_instance_index_defined) {
				ScriptLanguage::CodeCompletionOption option("instance_index", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
				option.insert_text = "instance_index(0)";
				r_options->push_back(option);
			}
		} break;
	}

	return ERR_PARSE_ERROR;
}

String ShaderLanguage::get_error_text() {
	return error_str;
}

Vector<ShaderLanguage::FilePosition> ShaderLanguage::get_include_positions() {
	return include_positions;
}

int ShaderLanguage::get_error_line() {
	return error_line;
}

ShaderLanguage::ShaderNode *ShaderLanguage::get_shader() {
	return shader;
}

ShaderLanguage::ShaderLanguage() {
	nodes = nullptr;
	completion_class = TAG_GLOBAL;

	if (instance_counter == 0) {
		int idx = 0;
		while (builtin_func_defs[idx].name) {
			if (builtin_func_defs[idx].tag == SubClassTag::TAG_GLOBAL) {
				global_func_set.insert(builtin_func_defs[idx].name);
			}
			idx++;
		}
	}
	instance_counter++;

#ifdef DEBUG_ENABLED
	warnings_check_map.insert(ShaderWarning::UNUSED_CONSTANT, &used_constants);
	warnings_check_map.insert(ShaderWarning::UNUSED_FUNCTION, &used_functions);
	warnings_check_map.insert(ShaderWarning::UNUSED_STRUCT, &used_structs);
	warnings_check_map.insert(ShaderWarning::UNUSED_UNIFORM, &used_uniforms);
	warnings_check_map.insert(ShaderWarning::UNUSED_VARYING, &used_varyings);

	warnings_check_map2.insert(ShaderWarning::UNUSED_LOCAL_VARIABLE, &used_local_vars);
#endif // DEBUG_ENABLED
}

ShaderLanguage::~ShaderLanguage() {
	clear();
	instance_counter--;
	if (instance_counter == 0) {
		global_func_set.clear();
	}
}
