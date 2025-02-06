/**************************************************************************/
/*  shader_converter.cpp                                                  */
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

#include "core/error/error_macros.h"
#include "core/templates/pair.h"
#include "core/typedefs.h"
#ifndef DISABLE_DEPRECATED

#include "shader_converter.h"

#define SL ShaderLanguage

const char *ShaderDeprecatedConverter::old_builtin_funcs[]{
	"abs",
	"acos",
	"acosh",
	"all",
	"any",
	"asin",
	"asinh",
	"atan",
	"atanh",
	"bool",
	"bvec2",
	"bvec3",
	"bvec4",
	"ceil",
	"clamp",
	"cos",
	"cosh",
	"cross",
	"dFdx",
	"dFdy",
	"degrees",
	"determinant",
	"distance",
	"dot",
	"equal",
	"exp",
	"exp2",
	"faceforward",
	"float",
	"floatBitsToInt",
	"floatBitsToUint",
	"floor",
	"fract",
	"fwidth",
	"greaterThan",
	"greaterThanEqual",
	"int",
	"intBitsToFloat",
	"inverse",
	"inversesqrt",
	"isinf",
	"isnan",
	"ivec2",
	"ivec3",
	"ivec4",
	"length",
	"lessThan",
	"lessThanEqual",
	"log",
	"log2",
	"mat2",
	"mat3",
	"mat4",
	"matrixCompMult",
	"max",
	"min",
	"mix",
	"mod",
	"modf",
	"normalize",
	"not",
	"notEqual",
	"outerProduct",
	"pow",
	"radians",
	"reflect",
	"refract",
	"round",
	"roundEven",
	"sign",
	"sin",
	"sinh",
	"smoothstep",
	"sqrt",
	"step",
	"tan",
	"tanh",
	"texelFetch",
	"texture",
	"textureGrad",
	"textureLod",
	"textureProj",
	"textureProjLod",
	"textureSize",
	"transpose",
	"trunc",
	"uint",
	"uintBitsToFloat",
	"uvec2",
	"uvec3",
	"uvec4",
	"vec2",
	"vec3",
	"vec4",
	nullptr
};

const ShaderDeprecatedConverter::RenamedBuiltins ShaderDeprecatedConverter::renamed_builtins[] = {
	{ "ALPHA_SCISSOR", "ALPHA_SCISSOR_THRESHOLD", { { RS::SHADER_SPATIAL, { "fragment" } } }, false },
	{ "CAMERA_MATRIX", "INV_VIEW_MATRIX", { { RS::SHADER_SPATIAL, { "vertex", "fragment", "light" } } }, false },
	{ "INV_CAMERA_MATRIX", "VIEW_MATRIX", { { RS::SHADER_SPATIAL, { "vertex", "fragment", "light" } } }, false },
	{ "NORMALMAP", "NORMAL_MAP", { { RS::SHADER_CANVAS_ITEM, { "fragment" } }, { RS::SHADER_SPATIAL, { "fragment" } } }, false },
	{ "NORMALMAP_DEPTH", "NORMAL_MAP_DEPTH", { { RS::SHADER_CANVAS_ITEM, { "fragment" } }, { RS::SHADER_SPATIAL, { "fragment" } } }, false },
	{ "TRANSMISSION", "BACKLIGHT", { { RS::SHADER_SPATIAL, { "fragment", "light" } } }, false },
	{ "WORLD_MATRIX", "MODEL_MATRIX", { { RS::SHADER_CANVAS_ITEM, { "vertex" } }, { RS::SHADER_SPATIAL, { "vertex", "fragment", "light" } } }, false },
	{ "CLEARCOAT_GLOSS", "CLEARCOAT_ROUGHNESS", { { RS::SHADER_SPATIAL, { "fragment" } } }, true }, // Usages require inversion, manually handled
	{ "INDEX", "INDEX", { { RS::SHADER_PARTICLES, { "vertex" } } }, true }, // No rename, was previously an int (vs. uint), usages require wrapping in `int()`.
	{ nullptr, nullptr, {}, false },
};

const ShaderDeprecatedConverter::RenamedRenderModes ShaderDeprecatedConverter::renamed_render_modes[] = {
	{ RS::SHADER_SPATIAL, "depth_draw_alpha_prepass", "depth_prepass_alpha" },
	{ RS::SHADER_MAX, nullptr, nullptr },
};

const ShaderDeprecatedConverter::RenamedHints ShaderDeprecatedConverter::renamed_hints[]{
	{ "hint_albedo", SL::TokenType::TK_HINT_SOURCE_COLOR },
	{ "hint_aniso", SL::TokenType::TK_HINT_ANISOTROPY_TEXTURE },
	{ "hint_black", SL::TokenType::TK_HINT_DEFAULT_BLACK_TEXTURE },
	{ "hint_black_albedo", SL::TokenType::TK_HINT_DEFAULT_BLACK_TEXTURE },
	{ "hint_color", SL::TokenType::TK_HINT_SOURCE_COLOR },
	{ "hint_transparent", SL::TokenType::TK_HINT_DEFAULT_TRANSPARENT_TEXTURE },
	{ "hint_white", SL::TokenType::TK_HINT_DEFAULT_WHITE_TEXTURE },
	{ nullptr, {} },
};

const ShaderDeprecatedConverter::RenamedFunctions ShaderDeprecatedConverter::renamed_functions[]{
	{ RS::SHADER_PARTICLES, SL::TK_TYPE_VOID, 0, "vertex", "process" },
	{ RS::SHADER_MAX, SL::TK_EMPTY, 0, nullptr, nullptr },
};

const ShaderDeprecatedConverter::RemovedRenderModes ShaderDeprecatedConverter::removed_render_modes[]{
	{ RS::SHADER_SPATIAL, "specular_blinn", false },
	{ RS::SHADER_SPATIAL, "specular_phong", false },
	{ RS::SHADER_SPATIAL, "async_visible", true },
	{ RS::SHADER_SPATIAL, "async_hidden", true },
	{ RS::SHADER_MAX, nullptr, false },
};

// These necessitate adding a uniform to the shader.
const ShaderDeprecatedConverter::RemovedBuiltins ShaderDeprecatedConverter::removed_builtins[]{
	{ "SCREEN_TEXTURE", SL::TK_TYPE_SAMPLER2D, { SL::TK_HINT_SCREEN_TEXTURE, SL::TK_FILTER_LINEAR_MIPMAP }, { { RS::SHADER_SPATIAL, { "fragment" } }, { RS::SHADER_CANVAS_ITEM, { "fragment" } } } },
	{ "DEPTH_TEXTURE", SL::TK_TYPE_SAMPLER2D, { SL::TK_HINT_DEPTH_TEXTURE, SL::TK_FILTER_LINEAR_MIPMAP }, { { RS::SHADER_SPATIAL, { "fragment" } } } },
	{ "NORMAL_ROUGHNESS_TEXTURE", SL::TK_TYPE_SAMPLER2D, { SL::TK_HINT_NORMAL_ROUGHNESS_TEXTURE, SL::TK_FILTER_LINEAR_MIPMAP }, { { RS::SHADER_SPATIAL, { "fragment" } } } },
	{ "MODULATE", SL::TK_ERROR, {}, { { RS::SHADER_CANVAS_ITEM, { "vertex", "fragment", "light" } } } }, // TODO: remove this when the MODULATE PR lands.
	{ nullptr, SL::TK_EMPTY, {}, {} },
};

const char *ShaderDeprecatedConverter::removed_types[]{
	nullptr,
};

HashSet<String> ShaderDeprecatedConverter::_new_builtin_funcs = HashSet<String>();

HashSet<String> ShaderDeprecatedConverter::_construct_new_builtin_funcs() {
	List<String> current_builtin_funcs;
	ShaderLanguage::get_builtin_funcs(&current_builtin_funcs);
	HashSet<String> old_funcs;
	for (int i = 0; old_builtin_funcs[i] != nullptr; i++) {
		old_funcs.insert(old_builtin_funcs[i]);
	}
	HashSet<String> new_funcs;
	for (List<String>::Element *E = current_builtin_funcs.front(); E; E = E->next()) {
		if (!old_funcs.has(E->get())) {
			new_funcs.insert(E->get());
		}
	}
	return new_funcs;
}

String ShaderDeprecatedConverter::get_builtin_rename(const String &p_name) {
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		if (renamed_builtins[i].name == p_name) {
			return renamed_builtins[i].replacement;
		}
	}
	return String();
}

bool ShaderDeprecatedConverter::has_builtin_rename(RS::ShaderMode p_mode, const String &p_name, const String &p_function) {
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		if (renamed_builtins[i].name == p_name) {
			for (int j = 0; j < renamed_builtins[i].mode_functions.size(); j++) {
				if (renamed_builtins[i].mode_functions[j].first == p_mode) {
					if (p_function == "") { // Empty function means don't check function.
						return true;
					}
					for (int k = 0; k < renamed_builtins[i].mode_functions[j].second.size(); k++) {
						if (renamed_builtins[i].mode_functions[j].second[k] == p_function) {
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

SL::TokenType ShaderDeprecatedConverter::get_removed_builtin_uniform_type(const String &p_name) {
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		if (removed_builtins[i].name == p_name) {
			return removed_builtins[i].uniform_type;
		}
	}
	return SL::TK_EMPTY;
}

Vector<SL::TokenType> ShaderDeprecatedConverter::get_removed_builtin_hints(const String &p_name) {
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		if (removed_builtins[i].name == p_name) {
			return removed_builtins[i].hints;
		}
	}
	return Vector<SL::TokenType>();
}

bool ShaderDeprecatedConverter::_rename_has_special_handling(const String &p_name) {
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		if (renamed_builtins[i].name == p_name) {
			return renamed_builtins[i].special_handling;
		}
	}
	return false;
}

void ShaderDeprecatedConverter::_get_builtin_renames_list(List<String> *r_list) {
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		r_list->push_back(renamed_builtins[i].name);
	}
}

void ShaderDeprecatedConverter::_get_render_mode_renames_list(List<String> *r_list) {
	for (int i = 0; renamed_render_modes[i].name != nullptr; i++) {
		r_list->push_back(renamed_render_modes[i].name);
	}
}

void ShaderDeprecatedConverter::_get_hint_renames_list(List<String> *r_list) {
	for (int i = 0; renamed_hints[i].name != nullptr; i++) {
		r_list->push_back(renamed_hints[i].name);
	}
}

void ShaderDeprecatedConverter::_get_function_renames_list(List<String> *r_list) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		r_list->push_back(renamed_functions[i].name);
	}
}

void ShaderDeprecatedConverter::_get_render_mode_removals_list(List<String> *r_list) {
	for (int i = 0; removed_render_modes[i].name != nullptr; i++) {
		r_list->push_back(removed_render_modes[i].name);
	}
}

void ShaderDeprecatedConverter::_get_builtin_removals_list(List<String> *r_list) {
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		r_list->push_back(removed_builtins[i].name);
	}
}

void ShaderDeprecatedConverter::_get_type_removals_list(List<String> *r_list) {
	for (int i = 0; removed_types[i] != nullptr; i++) {
		r_list->push_back(removed_types[i]);
	}
}

Vector<String> ShaderDeprecatedConverter::_get_funcs_builtin_rename(RS::ShaderMode p_mode, const String &p_name) {
	Vector<String> funcs;
	for (int i = 0; renamed_builtins[i].name != nullptr; i++) {
		if (renamed_builtins[i].name == p_name) {
			for (int j = 0; j < renamed_builtins[i].mode_functions.size(); j++) {
				if (renamed_builtins[i].mode_functions[j].first == p_mode) {
					for (int k = 0; k < renamed_builtins[i].mode_functions[j].second.size(); k++) {
						funcs.push_back(renamed_builtins[i].mode_functions[j].second[k]);
					}
				}
			}
		}
	}
	return funcs;
}

Vector<String> ShaderDeprecatedConverter::_get_funcs_builtin_removal(RS::ShaderMode p_mode, const String &p_name) {
	Vector<String> funcs;
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		if (removed_builtins[i].name == p_name) {
			for (int j = 0; j < removed_builtins[i].mode_functions.size(); j++) {
				if (removed_builtins[i].mode_functions[j].first == p_mode) {
					for (int k = 0; k < removed_builtins[i].mode_functions[j].second.size(); k++) {
						funcs.push_back(removed_builtins[i].mode_functions[j].second[k]);
					}
				}
			}
		}
	}
	return funcs;
}

bool ShaderDeprecatedConverter::is_removed_builtin(RS::ShaderMode p_mode, const String &p_name, const String &p_function) {
	for (int i = 0; removed_builtins[i].name != nullptr; i++) {
		if (removed_builtins[i].name == p_name) {
			for (int j = 0; j < removed_builtins[i].mode_functions.size(); j++) {
				if (removed_builtins[i].mode_functions[j].first == p_mode) {
					if (p_function == "") { // Empty function means don't check function.
						return true;
					}
					for (int k = 0; k < removed_builtins[i].mode_functions[j].second.size(); k++) {
						if (removed_builtins[i].mode_functions[j].second[k] == p_function) {
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool ShaderDeprecatedConverter::has_hint_replacement(const String &p_name) {
	for (int i = 0; renamed_hints[i].name != nullptr; i++) {
		if (renamed_hints[i].name == p_name) {
			return true;
		}
	}
	return false;
}

SL::TokenType ShaderDeprecatedConverter::get_hint_replacement(const String &p_name) {
	for (int i = 0; renamed_hints[i].name != nullptr; i++) {
		if (renamed_hints[i].name == p_name) {
			return renamed_hints[i].replacement;
		}
	}
	return {};
}

bool ShaderDeprecatedConverter::is_renamed_render_mode(RS::ShaderMode p_mode, const String &p_name) {
	for (int i = 0; renamed_render_modes[i].name != nullptr; i++) {
		if (renamed_render_modes[i].mode == p_mode && renamed_render_modes[i].name == p_name) {
			return true;
		}
	}
	return false;
}

String ShaderDeprecatedConverter::get_render_mode_rename(const String &p_name) {
	for (int i = 0; renamed_render_modes[i].name != nullptr; i++) {
		if (renamed_render_modes[i].name == p_name) {
			return renamed_render_modes[i].replacement;
		}
	}
	return {};
}

bool ShaderDeprecatedConverter::is_renamed_main_function(RS::ShaderMode p_mode, const String &p_name) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		if (renamed_functions[i].mode == p_mode && renamed_functions[i].name == p_name) {
			return true;
		}
	}
	return false;
}

bool ShaderDeprecatedConverter::is_renamee_main_function(RS::ShaderMode p_mode, const String &p_name) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		if (renamed_functions[i].mode == p_mode && renamed_functions[i].replacement == p_name) {
			return true;
		}
	}
	return false;
}

SL::TokenType ShaderDeprecatedConverter::get_renamed_function_type(const String &p_name) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		if (renamed_functions[i].name == p_name) {
			return renamed_functions[i].type;
		}
	}
	return SL::TK_MAX;
}

int ShaderDeprecatedConverter::get_renamed_function_arg_count(const String &p_name) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		if (renamed_functions[i].name == p_name) {
			return renamed_functions[i].arg_count;
		}
	}
	return -1;
}

bool ShaderDeprecatedConverter::FunctionDecl::is_renamed_main_function(RS::ShaderMode p_mode) const {
	if (!name_pos || !type_pos) {
		return false;
	}
	if (ShaderDeprecatedConverter::is_renamed_main_function(p_mode, name_pos->get().text) && type_pos->get().type == get_renamed_function_type(name_pos->get().text) && arg_count == get_renamed_function_arg_count(name_pos->get().text)) {
		return true;
	}
	return false;
}

bool ShaderDeprecatedConverter::FunctionDecl::is_new_main_function(RS::ShaderMode p_mode) const {
	if (!name_pos || !type_pos) {
		return false;
	}
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		if (renamed_functions[i].mode == p_mode && renamed_functions[i].replacement == name_pos->get().text && renamed_functions[i].type == type_pos->get().type && arg_count == renamed_functions[i].arg_count) {
			return true;
		}
	}
	return false;
}

String ShaderDeprecatedConverter::get_main_function_rename(const String &p_name) {
	for (int i = 0; renamed_functions[i].name != nullptr; i++) {
		if (renamed_functions[i].name == p_name) {
			return renamed_functions[i].replacement;
		}
	}
	return String();
}

bool ShaderDeprecatedConverter::has_removed_render_mode(RS::ShaderMode p_mode, const String &p_name) {
	for (int i = 0; removed_render_modes[i].name != nullptr; i++) {
		if (removed_render_modes[i].mode == p_mode && removed_render_modes[i].name == p_name) {
			return true;
		}
	}
	return false;
}

bool ShaderDeprecatedConverter::can_remove_render_mode(const String &p_name) {
	for (int i = 0; removed_render_modes[i].name != nullptr; i++) {
		if (removed_render_modes[i].name == p_name) {
			return removed_render_modes[i].can_remove;
		}
	}
	return false;
}

bool ShaderDeprecatedConverter::has_removed_type(const String &p_name) {
	for (int i = 0; removed_types[i] != nullptr; i++) {
		if (removed_types[i] == p_name) {
			return true;
		}
	}
	return false;
}

static constexpr const char *token_to_str[] = {
	"", // TK_EMPTY
	"", // TK_IDENTIFIER
	"true",
	"false",
	"", // TK_FLOAT_CONSTANT
	"", // TK_INT_CONSTANT
	"", // TK_UINT_CONSTANT
	"", // TK_STRING_CONSTANT
	"void",
	"bool",
	"bvec2",
	"bvec3",
	"bvec4",
	"int",
	"ivec2",
	"ivec3",
	"ivec4",
	"uint",
	"uvec2",
	"uvec3",
	"uvec4",
	"float",
	"vec2",
	"vec3",
	"vec4",
	"mat2",
	"mat3",
	"mat4",
	"sampler2D",
	"isampler2D",
	"usampler2D",
	"sampler2DArray",
	"isampler2DArray",
	"usampler2DArray",
	"sampler3D",
	"isampler3D",
	"usampler3D",
	"samplerCube",
	"samplerCubeArray",
	"samplerExternalOES",
	"flat",
	"smooth",
	"const",
	"struct",
	"lowp",
	"mediump",
	"highp",
	"==",
	"!=",
	"<",
	"<=",
	">",
	">=",
	"&&",
	"||",
	"!",
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
	"if",
	"else",
	"for",
	"while",
	"do",
	"switch",
	"case",
	"default",
	"break",
	"continue",
	"return",
	"discard",
	"[",
	"]",
	"{",
	"}",
	"(",
	")",
	"?",
	",",
	":",
	";",
	".",
	"uniform",
	"group_uniforms",
	"instance",
	"global",
	"varying",
	"in",
	"out",
	"inout",
	"render_mode",
	"hint_default_white",
	"hint_default_black",
	"hint_default_transparent",
	"hint_normal",
	"hint_roughness_normal",
	"hint_roughness_r",
	"hint_roughness_g",
	"hint_roughness_b",
	"hint_roughness_a",
	"hint_roughness_gray",
	"hint_anisotropy",
	"source_color",
	"hint_range",
	"hint_enum",
	"instance_index",
	"hint_screen_texture",
	"hint_normal_roughness_texture",
	"hint_depth_texture",
	"filter_nearest",
	"filter_linear",
	"filter_nearest_mipmap",
	"filter_linear_mipmap",
	"filter_nearest_mipmap_anisotropic",
	"filter_linear_mipmap_anisotropic",
	"repeat_enable",
	"repeat_disable",
	"shader_type",
	"", // TK_CURSOR
	"", // TK_ERROR
	"", // TK_EOF
	"\t",
	"\r",
	" ",
	"\n",
	"", // TK_BLOCK_COMMENT
	"", // TK_LINE_COMMENT
	"", // TK_PREPROC_DIRECTIVE
};
static_assert(ShaderLanguage::TK_MAX == sizeof(token_to_str) / sizeof(token_to_str[0]), "token_to_str length does not match token count (Did TK_MAX change?)");

bool ShaderDeprecatedConverter::token_is_skippable(const Token &p_tk) {
	switch (p_tk.type) {
		case ShaderLanguage::TK_TAB:
		case ShaderLanguage::TK_CR:
		case ShaderLanguage::TK_SPACE:
		case ShaderLanguage::TK_NEWLINE:
		case ShaderLanguage::TK_BLOCK_COMMENT:
		case ShaderLanguage::TK_LINE_COMMENT:
		case ShaderLanguage::TK_PREPROC_DIRECTIVE:
			return true;
		default:
			break;
	}
	return false;
}

List<SL::Token>::Element *ShaderDeprecatedConverter::_get_next_token_ptr(List<Token>::Element *p_curr_ptr) const {
	ERR_FAIL_COND_V(p_curr_ptr == nullptr, p_curr_ptr);
	if (p_curr_ptr->next() == nullptr) {
		return p_curr_ptr;
	}
	p_curr_ptr = p_curr_ptr->next();
	while (token_is_skippable(p_curr_ptr->get())) {
		if (p_curr_ptr->next() == nullptr) {
			return p_curr_ptr;
		}
		p_curr_ptr = p_curr_ptr->next();
	}
	return p_curr_ptr;
}

List<SL::Token>::Element *ShaderDeprecatedConverter::_get_prev_token_ptr(List<Token>::Element *_curr_ptr) const {
	ERR_FAIL_COND_V(_curr_ptr == nullptr, _curr_ptr);
	if (_curr_ptr->prev() == nullptr) {
		return _curr_ptr;
	}
	_curr_ptr = _curr_ptr->prev();
	while (token_is_skippable(_curr_ptr->get())) {
		if (_curr_ptr->prev() == nullptr) {
			return _curr_ptr;
		}
		_curr_ptr = _curr_ptr->prev();
	}
	return _curr_ptr;
}

List<SL::Token>::Element *ShaderDeprecatedConverter::get_next_token() {
	curr_ptr = _get_next_token_ptr(curr_ptr);
	return curr_ptr;
}

List<SL::Token>::Element *ShaderDeprecatedConverter::get_prev_token() {
	curr_ptr = _get_prev_token_ptr(curr_ptr);
	return curr_ptr;
}

List<SL::Token>::Element *ShaderDeprecatedConverter::remove_cur_and_get_next() {
	ERR_FAIL_COND_V(!curr_ptr, nullptr);
	List<Token>::Element *prev = curr_ptr->prev();
	if (!prev) {
		prev = curr_ptr->next();
		code_tokens.erase(curr_ptr);
		while (token_is_skippable(prev->get())) {
			if (prev->next() == nullptr) {
				return prev;
			}
			prev = prev->next();
		}
		return prev;
	}
	code_tokens.erase(curr_ptr);
	curr_ptr = prev;
	return get_next_token();
}

SL::TokenType ShaderDeprecatedConverter::_peek_tk_type(int64_t p_count, List<Token>::Element **r_pos) const {
	ERR_FAIL_COND_V(!curr_ptr, ShaderLanguage::TK_EOF);
	if (p_count == 0) {
		return curr_ptr->get().type;
	}

	bool backwards = p_count < 0;
	uint64_t max_count = abs(p_count);
	TokenE *start_ptr = curr_ptr;
	for (uint64_t i = 0; i < max_count; i++) {
		TokenE *_ptr = backwards ? _get_prev_token_ptr(start_ptr) : _get_next_token_ptr(start_ptr);
		if (!_ptr) {
			if (r_pos) {
				*r_pos = start_ptr;
			}
			return ShaderLanguage::TK_EOF;
		}
		start_ptr = _ptr;
	}
	if (r_pos) {
		*r_pos = start_ptr;
	}
	return start_ptr->get().type;
}

bool ShaderDeprecatedConverter::scope_has_decl(const String &p_scope, const String &p_name) const {
	if (uniform_decls.has(p_name) ||
			(scope_declarations.has("<global>") && scope_declarations["<global>"].has(p_name)) ||
			(scope_declarations.has(p_scope) && scope_declarations[p_scope].has(p_name))) {
		return true;
	}
	return false;
}

SL::TokenType ShaderDeprecatedConverter::peek_next_tk_type(uint32_t p_count) const {
	return _peek_tk_type(p_count);
}

SL::TokenType ShaderDeprecatedConverter::peek_prev_tk_type(uint32_t p_count) const {
	return _peek_tk_type(-((int64_t)p_count));
}

List<SL::Token>::Element *ShaderDeprecatedConverter::get_pos() const {
	ERR_FAIL_COND_V(!curr_ptr, nullptr);
	return curr_ptr;
}

bool ShaderDeprecatedConverter::reset_to(List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	curr_ptr = p_pos;
	return true;
}

bool ShaderDeprecatedConverter::insert_after(const Vector<Token> &p_token_list, List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	for (int i = p_token_list.size() - 1; i >= 0; i--) {
		const Token &tk = p_token_list[i];
		code_tokens.insert_after(p_pos, { tk.type, tk.text, tk.constant, tk.line, tk.length, NEW_IDENT });
	}
	return true;
}

bool ShaderDeprecatedConverter::insert_before(const Vector<Token> &p_token_list, List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	for (const Token &tk : p_token_list) {
		code_tokens.insert_before(p_pos, { tk.type, tk.text, tk.constant, tk.line, tk.length, NEW_IDENT });
	}
	return true;
}

bool ShaderDeprecatedConverter::insert_after(const Token &p_token, List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	Token new_token = p_token;
	new_token.pos = NEW_IDENT;
	code_tokens.insert_after(p_pos, new_token);
	return true;
}

bool ShaderDeprecatedConverter::insert_before(const Token &p_token, List<Token>::Element *p_pos) {
	ERR_FAIL_COND_V(p_pos == nullptr, false);
	Token new_token = p_token;
	new_token.pos = NEW_IDENT;
	code_tokens.insert_before(p_pos, new_token);
	return true;
}

List<SL::Token>::Element *ShaderDeprecatedConverter::replace_curr(const Token &p_token, String comment_format) {
	ERR_FAIL_COND_V(curr_ptr == nullptr, nullptr);
	Token new_token = p_token;
	new_token.pos = NEW_IDENT;
	List<Token>::Element *prev = curr_ptr;
	if (!comment_format.is_empty()) {
		int count = comment_format.count("%s");
		if (count == 0) {
			comment_format = RTR(comment_format) + " '%s' renamed to '%s'";
		} else if (count == 1) {
			comment_format = RTR(comment_format) + ", renamed to '%s'";
		}
		_add_comment_before(vformat(RTR(comment_format), get_token_literal_text(curr_ptr->get()), p_token.text), curr_ptr, false);
	}
	curr_ptr = code_tokens.insert_before(curr_ptr, new_token);
	ERR_FAIL_COND_V(!code_tokens.erase(prev), nullptr);
	return curr_ptr;
}

SL::Token ShaderDeprecatedConverter::mkTok(TokenType p_type, const StringName &p_text, double p_constant, uint16_t p_line) {
	return { p_type, p_text, p_constant, p_line, 0, NEW_IDENT };
}

bool ShaderDeprecatedConverter::_insert_uniform_declaration(const String &p_name) {
	if (after_shader_decl == nullptr) {
		return false;
	}
	TokenType type = get_removed_builtin_uniform_type(p_name);
	Vector<TokenType> hints = get_removed_builtin_hints(p_name);
	Vector<Token> uni_decl = { mkTok(TT::TK_NEWLINE), mkTok(TT::TK_UNIFORM), mkTok(TT::TK_SPACE), mkTok(type),
		mkTok(TT::TK_SPACE), mkTok(TT::TK_IDENTIFIER, p_name), mkTok(TT::TK_SPACE), mkTok(TT::TK_COLON),
		mkTok(TT::TK_SPACE) };
	for (int i = 0; i < hints.size(); i++) {
		uni_decl.append(mkTok(hints[i]));
		if (i < hints.size() - 1) {
			uni_decl.append(mkTok(TT::TK_COMMA));
			uni_decl.append(mkTok(TT::TK_SPACE));
		}
	}
	uni_decl.append_array({ mkTok(TT::TK_SEMICOLON), mkTok(TT::TK_NEWLINE) });
	if (!insert_after(uni_decl, after_shader_decl)) {
		return false;
	}
	TokenE *cur_pos = get_pos();
	reset_to(after_shader_decl);
	UniformDecl uni;
	uni.start_pos = get_next_token(); // uniform
	uni.type_pos = get_next_token(); // type
	uni.name_pos = get_next_token(); // id
	get_next_token(); // colon
	for (int i = 0; i < hints.size(); i++) {
		uni.hint_poses.push_back(get_next_token()); // hint
		if (i < hints.size() - 1) {
			get_next_token(); // comma
		}
	}
	uni.end_pos = get_next_token();
	uniform_decls[p_name] = uni;
	_add_comment_before(vformat(RTR("Usage of deprecated built-in '%s' requires a uniform declaration."), p_name), uni.start_pos, false);
	reset_to(cur_pos);
	return true;
}

RS::ShaderMode ShaderDeprecatedConverter::get_shader_mode_from_string(const String &p_mode) {
	if (p_mode == "spatial") {
		return RS::SHADER_SPATIAL;
	} else if (p_mode == "canvas_item") {
		return RS::SHADER_CANVAS_ITEM;
	} else if (p_mode == "particles") {
		return RS::SHADER_PARTICLES;
	} else { // 3.x didn't support anything else.
		return RS::SHADER_MAX;
	}
}
// Remove from the current token to end (exclsusive) and return the new current token.
List<SL::Token>::Element *ShaderDeprecatedConverter::_remove_from_curr_to(TokenE *p_end) {
	ERR_FAIL_COND_V(p_end == nullptr, nullptr);
	while (curr_ptr != p_end) {
		TokenE *next = curr_ptr->next();
		code_tokens.erase(curr_ptr);
		curr_ptr = next;
	}
	return curr_ptr;
}

SL::TokenType ShaderDeprecatedConverter::get_tokentype_from_text(const String &p_text) {
	for (int i = 0; i < SL::TK_MAX; i++) {
		if (token_to_str[i] == p_text) {
			return static_cast<SL::TokenType>(i);
		}
	}
	return SL::TK_MAX;
}

String ShaderDeprecatedConverter::get_tokentype_text(TokenType p_tk_type) {
	return token_to_str[p_tk_type];
}

List<SL::Token>::Element *ShaderDeprecatedConverter::_get_end_of_closure() {
	int additional_closures = 0;
	TokenE *ptr = curr_ptr;
	bool start_is_scope_start = false;
	switch (ptr->get().type) {
		case TT::TK_CURLY_BRACKET_OPEN:
		case TT::TK_PARENTHESIS_OPEN:
		case TT::TK_BRACKET_OPEN:
			start_is_scope_start = true;
			break;
		default:
			break;
	}
	for (; ptr; ptr = ptr->next()) {
		switch (ptr->get().type) {
			case TT::TK_CURLY_BRACKET_OPEN:
			case TT::TK_PARENTHESIS_OPEN:
			case TT::TK_BRACKET_OPEN: {
				additional_closures++;
			} break;
			case TT::TK_CURLY_BRACKET_CLOSE:
			case TT::TK_PARENTHESIS_CLOSE:
			case TT::TK_BRACKET_CLOSE: {
				if (additional_closures > 0) {
					additional_closures--;
					if (start_is_scope_start && additional_closures == 0) {
						return ptr;
					}
				} else {
					return ptr;
				}
			} break;
			case TT::TK_SEMICOLON:
			case TT::TK_COMMA: {
				if (additional_closures <= 0) {
					return _get_prev_token_ptr(ptr);
				}
			} break;
			case TT::TK_EOF:
			case TT::TK_ERROR: {
				err_line = curr_ptr->get().line + 1;
				err_str = ptr->get().type == TT::TK_ERROR ? vformat(RTR("Parser Error (%s) ", ptr->get().text)) : vformat(RTR("Could not find end of closure for token '%s'"), get_tokentype_text(curr_ptr->get().type));
				return ptr;
			} break;
			default:
				break;
		}
	}
	return ptr;
}

bool ShaderDeprecatedConverter::token_is_type(const Token &p_tk) {
	return (ShaderLanguage::is_token_datatype(p_tk.type)) || struct_decls.has(get_token_literal_text(p_tk)) || (p_tk.type == TT::TK_IDENTIFIER && (has_removed_type(p_tk.text)));
}

bool ShaderDeprecatedConverter::token_is_hint(const Token &p_tk) {
	if (p_tk.type == TT::TK_IDENTIFIER) {
		return has_hint_replacement(p_tk.text);
	}
	return SL::is_token_hint(p_tk.type);
}

String ShaderDeprecatedConverter::get_token_literal_text(const Token &p_tk) const {
	switch (p_tk.type) {
		case TT::TK_PREPROC_DIRECTIVE:
		case TT::TK_LINE_COMMENT:
		case TT::TK_BLOCK_COMMENT:
		case TT::TK_IDENTIFIER: { // Identifiers prefixed with `__` are modified to `_dup_` by the SL parser
			if (p_tk.pos == NEW_IDENT) {
				return p_tk.text;
			} else {
				return old_code.substr(p_tk.pos, p_tk.length);
			}
		} break;
		case TT::TK_INT_CONSTANT:
		case TT::TK_FLOAT_CONSTANT:
		case TT::TK_UINT_CONSTANT: {
			if (p_tk.pos == NEW_IDENT) {
				// Fix for 3.x float constants not having a decimal point.
				if (!p_tk.is_integer_constant() && p_tk.text != "") {
					return p_tk.text;
				}
				String const_str = rtos(p_tk.constant);
				if (!p_tk.is_integer_constant() && !const_str.contains(".")) {
					const_str += ".0";
				}
				return const_str;
			} else {
				return old_code.substr(p_tk.pos, p_tk.length);
			}
		} break;
		case TT::TK_ERROR:
		case TT::TK_EOF: {
			return "";
		} break;
		default:
			break;
	}
	return token_to_str[p_tk.type];
}

bool ShaderDeprecatedConverter::tokentype_is_identifier(const TokenType &p_tk_type) {
	return p_tk_type == TT::TK_IDENTIFIER || tokentype_is_new_reserved_keyword(p_tk_type);
}

bool ShaderDeprecatedConverter::tokentype_is_new_type(const TokenType &p_type) {
	// the following types are in both 3.x and 4.x
	switch (p_type) {
		case TT::TK_TYPE_VOID:
		case TT::TK_TYPE_BOOL:
		case TT::TK_TYPE_BVEC2:
		case TT::TK_TYPE_BVEC3:
		case TT::TK_TYPE_BVEC4:
		case TT::TK_TYPE_INT:
		case TT::TK_TYPE_IVEC2:
		case TT::TK_TYPE_IVEC3:
		case TT::TK_TYPE_IVEC4:
		case TT::TK_TYPE_UINT:
		case TT::TK_TYPE_UVEC2:
		case TT::TK_TYPE_UVEC3:
		case TT::TK_TYPE_UVEC4:
		case TT::TK_TYPE_FLOAT:
		case TT::TK_TYPE_VEC2:
		case TT::TK_TYPE_VEC3:
		case TT::TK_TYPE_VEC4:
		case TT::TK_TYPE_MAT2:
		case TT::TK_TYPE_MAT3:
		case TT::TK_TYPE_MAT4:
		case TT::TK_TYPE_SAMPLER2D:
		case TT::TK_TYPE_ISAMPLER2D:
		case TT::TK_TYPE_USAMPLER2D:
		case TT::TK_TYPE_SAMPLER2DARRAY:
		case TT::TK_TYPE_ISAMPLER2DARRAY:
		case TT::TK_TYPE_USAMPLER2DARRAY:
		case TT::TK_TYPE_SAMPLER3D:
		case TT::TK_TYPE_ISAMPLER3D:
		case TT::TK_TYPE_USAMPLER3D:
		case TT::TK_TYPE_SAMPLERCUBE:
		case TT::TK_TYPE_SAMPLEREXT:
			return false;
		default:
			break;
	}
	return SL::is_token_datatype(p_type);
}

// checks for reserved keywords only found in 4.x
bool ShaderDeprecatedConverter::tokentype_is_new_reserved_keyword(const TokenType &tk_type) {
	switch (tk_type) {
		// The following keyword tokens are in both 3.x and 4.x.
		case TT::TK_ARG_IN:
		case TT::TK_ARG_INOUT:
		case TT::TK_ARG_OUT:
		case TT::TK_CF_BREAK:
		case TT::TK_CF_CASE:
		case TT::TK_CF_CONTINUE:
		case TT::TK_CF_DEFAULT:
		case TT::TK_CF_DISCARD:
		case TT::TK_CF_DO:
		case TT::TK_CF_ELSE:
		case TT::TK_CF_FOR:
		case TT::TK_CF_IF:
		case TT::TK_CF_RETURN:
		case TT::TK_CF_SWITCH:
		case TT::TK_CF_WHILE:
		case TT::TK_CONST:
		case TT::TK_ERROR:
		case TT::TK_FALSE:
		case TT::TK_HINT_NORMAL_TEXTURE:
		case TT::TK_HINT_RANGE:
		case TT::TK_INTERPOLATION_FLAT:
		case TT::TK_INTERPOLATION_SMOOTH:
		case TT::TK_PRECISION_HIGH:
		case TT::TK_PRECISION_LOW:
		case TT::TK_PRECISION_MID:
		case TT::TK_RENDER_MODE:
		case TT::TK_SHADER_TYPE:
		case TT::TK_STRUCT:
		case TT::TK_TRUE:
		case TT::TK_TYPE_BOOL:
		case TT::TK_TYPE_BVEC2:
		case TT::TK_TYPE_BVEC3:
		case TT::TK_TYPE_BVEC4:
		case TT::TK_TYPE_FLOAT:
		case TT::TK_TYPE_INT:
		case TT::TK_TYPE_ISAMPLER2D:
		case TT::TK_TYPE_ISAMPLER2DARRAY:
		case TT::TK_TYPE_ISAMPLER3D:
		case TT::TK_TYPE_IVEC2:
		case TT::TK_TYPE_IVEC3:
		case TT::TK_TYPE_IVEC4:
		case TT::TK_TYPE_MAT2:
		case TT::TK_TYPE_MAT3:
		case TT::TK_TYPE_MAT4:
		case TT::TK_TYPE_SAMPLER2D:
		case TT::TK_TYPE_SAMPLER2DARRAY:
		case TT::TK_TYPE_SAMPLER3D:
		case TT::TK_TYPE_SAMPLERCUBE:
		case TT::TK_TYPE_SAMPLEREXT:
		case TT::TK_TYPE_UINT:
		case TT::TK_TYPE_USAMPLER2D:
		case TT::TK_TYPE_USAMPLER2DARRAY:
		case TT::TK_TYPE_USAMPLER3D:
		case TT::TK_TYPE_UVEC2:
		case TT::TK_TYPE_UVEC3:
		case TT::TK_TYPE_UVEC4:
		case TT::TK_TYPE_VEC2:
		case TT::TK_TYPE_VEC3:
		case TT::TK_TYPE_VEC4:
		case TT::TK_TYPE_VOID:
		case TT::TK_UNIFORM:
		case TT::TK_VARYING:
		case TT::TK_MAX:
			return false;
		default:
			break;
	}
	return SL::is_token_keyword(tk_type);
}

bool ShaderDeprecatedConverter::tokentype_is_new_hint(const TokenType &tk_type) {
	switch (tk_type) {
		case TT::TK_HINT_NORMAL_TEXTURE: // These two are in both 3.x and 4.x.
		case TT::TK_HINT_RANGE:
			return false;
		default:
			break;
	}
	return SL::is_token_hint(tk_type);
}

bool ShaderDeprecatedConverter::id_is_new_builtin_func(const String &p_name) {
	if (_new_builtin_funcs.is_empty()) {
		_new_builtin_funcs = _construct_new_builtin_funcs();
	}
	return _new_builtin_funcs.has(p_name);
}

void ShaderDeprecatedConverter::_get_new_builtin_funcs_list(List<String> *r_list) {
	if (_new_builtin_funcs.is_empty()) {
		_new_builtin_funcs = _construct_new_builtin_funcs();
	}
	for (const String &k : _new_builtin_funcs) {
		r_list->push_back(k);
	}
}

String ShaderDeprecatedConverter::get_report() {
	String report_str;
	for (const KeyValue<int, Vector<String>> &p : report) {
		for (const String &v : p.value) {
			report_str += vformat(RTR("Line %d: %s\n"), p.key, v);
		}
	}
	return report_str;
}

bool ShaderDeprecatedConverter::_add_to_report(int p_line, const String &p_msg, int level) {
	String message = p_msg;
	if (level == 1) {
		message = "WARNING: " + message;
	} else if (level == 2) {
		message = "ERROR: " + message;
	}
	if (!report.has(p_line)) {
		report[p_line] = Vector<String>();
	}
	if (report[p_line].has(p_msg)) {
		return false;
	}
	report[p_line].push_back(p_msg);
	return true;
}

bool ShaderDeprecatedConverter::_add_comment_before(const String &p_comment, List<Token>::Element *p_pos, bool warning) {
	// Peek back until we hit a newline or the start of the file (EOF).
	TokenE *start_pos = p_pos;
	if (!start_pos) {
		return false;
	}
	if (!_add_to_report(start_pos->get().line, p_comment, warning ? 1 : 0)) {
		// Already added.
		return true;
	}
	while (start_pos->prev() && start_pos->get().type != TT::TK_NEWLINE && start_pos->get().type != TT::TK_EOF) {
		start_pos = start_pos->prev();
	}
	String block_comment = vformat("/* !convert%s */\n", (warning ? " WARNING: " : ": ") + p_comment);
	// In case this has been run through the converter before, check if the token before this is a block comment and has the same comment.
	TokenE *prev = start_pos->prev();
	while (prev && prev->get().type == TT::TK_BLOCK_COMMENT) {
		if (get_token_literal_text(start_pos->next()->get()).strip_edges() == block_comment.strip_edges()) {
			return true;
		}
		prev = prev->prev();
	}
	return insert_after(mkTok(TT::TK_BLOCK_COMMENT, block_comment), start_pos);
}

bool ShaderDeprecatedConverter::_add_comment_at_eol(const String &p_comment, List<Token>::Element *p_pos) {
	// Peek forward until we hit a newline or the end of the file (EOF).
	TokenE *start_pos = p_pos ? p_pos : get_pos();
	if (!start_pos) {
		return false;
	}
	while (start_pos->get().type != TT::TK_NEWLINE && start_pos->get().type != TT::TK_EOF) {
		start_pos = start_pos->next();
	}
	String comment = "/* !convert: " + p_comment + " */";
	if (start_pos->prev() && start_pos->prev()->get().type == TT::TK_BLOCK_COMMENT && get_token_literal_text(start_pos->prev()->get()) == comment) {
		return true;
	}
	return insert_before(mkTok(TT::TK_BLOCK_COMMENT, comment), start_pos);
}

void ShaderDeprecatedConverter::reset() {
	ShaderLanguage sl;
	code_tokens.clear();
	sl.token_debug_stream(old_code, code_tokens, true);
	code_tokens.push_back(eof_token);
	code_tokens.push_front(eof_token);
	uniform_decls.clear();
	var_decls.clear();
	struct_decls.clear();
	function_decls.clear();
	scope_declarations.clear();
	after_shader_decl = code_tokens.front();
	curr_ptr = code_tokens.front();
	report.clear();
}

#define COND_MSG_FAIL(m_cond, m_msg) \
	if (unlikely(m_cond)) {          \
		err_str = m_msg;             \
		return false;                \
	}
#define COND_LINE_MSG_FAIL(m_cond, m_line, m_msg) \
	if (unlikely(m_cond)) {                       \
		err_line = m_line + 1;                    \
		err_str = m_msg;                          \
		_add_to_report(err_line, err_str, 2);     \
		return false;                             \
	}
#define LINE_MSG_FAIL(m_line, m_msg) \
	err_line = m_line + 1;           \
	err_str = m_msg;                 \
	return false;
#define MSG_FAIL(m_msg) \
	err_str = m_msg;    \
	return false;

#define EOF_FAIL(m_tok_E)                                             \
	COND_MSG_FAIL(m_tok_E == nullptr, RTR("Unexpected end of file")); \
	COND_LINE_MSG_FAIL(m_tok_E->get().type == TT::TK_EOF || m_tok_E->get().type == TT::TK_ERROR, m_tok_E->get().line, m_tok_E->get().type == TT::TK_ERROR ? vformat(RTR("Parser Error (%s) ", m_tok_E->get().text)) : RTR("Unexpected end of file"));
#define CLOSURE_FAIL(m_tok_E)                                                                 \
	COND_MSG_FAIL(m_tok_E == nullptr, RTR("Unexpected end of file"));                         \
	if (unlikely(m_tok_E->get().type == TT::TK_EOF || m_tok_E->get().type == TT::TK_ERROR)) { \
		return false;                                                                         \
	}

// At uniform statement.
bool ShaderDeprecatedConverter::_parse_uniform() {
	UniformDecl uni;
	uni.start_pos = get_pos();
	DEV_ASSERT(uni.start_pos && uni.start_pos->get().type == TT::TK_UNIFORM);
	uni.uniform_stmt_pos = uni.start_pos;
	if (SL::is_token_uniform_qual(peek_prev_tk_type())) { // 3.x doesn't support these.
		uni.start_pos = get_prev_token();
		get_next_token(); // Back to the uniform.
	}
	TokenE *next_tk = get_next_token();
	EOF_FAIL(next_tk);
	while (SL::is_token_precision(next_tk->get().type) || SL::is_token_interpolation(next_tk->get().type)) {
		if (SL::is_token_interpolation(next_tk->get().type)) { // Interpolations are not supported for uniforms in newer versions of Godot.
			uni.interp_qual_pos = next_tk;
		}
		next_tk = get_next_token();
		EOF_FAIL(next_tk);
	}
	COND_LINE_MSG_FAIL(!token_is_type(next_tk->get()), next_tk->get().line, RTR("Expected type after 'uniform'"));
	uni.type_pos = next_tk;
	next_tk = get_next_token();
	EOF_FAIL(next_tk);
	if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
		uni.is_array = true;
		if (!_skip_array_size()) {
			return false;
		}
		next_tk = get_next_token();
		EOF_FAIL(next_tk);
	}
	COND_LINE_MSG_FAIL(!tokentype_is_identifier(next_tk->get().type), next_tk->get().line, RTR("Expected identifier after uniform type"));
	String name = get_token_literal_text(next_tk->get());
	uni.name_pos = next_tk;
	next_tk = get_next_token();
	EOF_FAIL(next_tk);
	if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
		uni.is_array = true;
		if (!_skip_array_size()) {
			return false;
		}
		next_tk = get_next_token();
		EOF_FAIL(next_tk);
	}
	if (next_tk->get().type == TT::TK_COLON) {
		while (true) {
			next_tk = get_next_token();
			EOF_FAIL(next_tk);
			COND_LINE_MSG_FAIL(!token_is_hint(next_tk->get()), next_tk->get().line, RTR("Expected hint after ':' in uniform declaration"));
			uni.hint_poses.push_back(next_tk);
			next_tk = get_next_token();
			EOF_FAIL(next_tk);
			if (next_tk->get().type == TT::TK_PARENTHESIS_OPEN) {
				next_tk = _get_end_of_closure();
				CLOSURE_FAIL(next_tk);
				COND_LINE_MSG_FAIL(next_tk->get().type != TT::TK_PARENTHESIS_CLOSE, next_tk->get().line, RTR("Expected ')' after hint range"));
				reset_to(next_tk); // Skip the hint range.
				next_tk = get_next_token();
				EOF_FAIL(next_tk);
			}
			if (next_tk->get().type != TT::TK_COMMA) {
				break;
			}
		}
	}
	if (next_tk->get().type == TT::TK_OP_ASSIGN) {
		next_tk = _get_end_of_closure();
		CLOSURE_FAIL(next_tk);
		reset_to(next_tk); // Skip the assignment.
		next_tk = get_next_token();
	}
	uni.end_pos = next_tk;
	EOF_FAIL(uni.end_pos);
	COND_LINE_MSG_FAIL(uni.end_pos->get().type != TT::TK_SEMICOLON, uni.end_pos->get().line, RTR("Expected ';' after uniform declaration"));
	uniform_decls[name] = uni;
	return true;
}

bool ShaderDeprecatedConverter::_skip_uniform() {
	TokenE *cur_tok = get_pos();
	DEV_ASSERT(cur_tok && cur_tok->get().type == TT::TK_UNIFORM);
	for (KeyValue<String, UniformDecl> &kv : uniform_decls) {
		if (kv.value.uniform_stmt_pos == cur_tok) {
			reset_to(kv.value.end_pos);
			return true;
		}
	}
	LINE_MSG_FAIL(cur_tok->get().line, RTR("Uniform declaration not found"));
}

bool ShaderDeprecatedConverter::_skip_array_size() {
	TokenE *next_tk = get_pos();
	DEV_ASSERT(next_tk && next_tk->get().type == TT::TK_BRACKET_OPEN);
	next_tk = _get_end_of_closure();
	CLOSURE_FAIL(next_tk);
	COND_LINE_MSG_FAIL(next_tk->get().type != TT::TK_BRACKET_CLOSE, next_tk->get().line, RTR("Expected ']' after array type"));
	reset_to(next_tk); // Skip to end.
	return true;
}

bool ShaderDeprecatedConverter::_skip_struct() {
	DEV_ASSERT(get_pos() && get_pos()->get().type == TT::TK_STRUCT);
	TokenE *struct_name = get_next_token();
	EOF_FAIL(struct_name);
	TokenE *struct_body_start;
	if (struct_name->get().type == TT::TK_CURLY_BRACKET_OPEN) {
		struct_body_start = struct_name;
	} else {
		struct_body_start = get_next_token();
	}
	EOF_FAIL(struct_body_start);
	COND_LINE_MSG_FAIL(struct_body_start->get().type != TT::TK_CURLY_BRACKET_OPEN, struct_body_start->get().line, RTR("Expected '{' after struct declaration"));
	TokenE *struct_body_end = _get_end_of_closure();
	CLOSURE_FAIL(struct_body_end);
	COND_LINE_MSG_FAIL(struct_body_end->get().type != TT::TK_CURLY_BRACKET_CLOSE, struct_body_start->get().line, RTR("Expected '}' bracket at end of struct declaration"));
	reset_to(struct_body_end);
	return true;
}

bool ShaderDeprecatedConverter::_tok_is_start_of_decl(const Token &p_tk) {
	return token_is_type(p_tk) || p_tk.type == TT::TK_CONST || p_tk.type == TT::TK_VARYING || SL::is_token_precision(p_tk.type) || SL::is_token_interpolation(p_tk.type);
}

bool ShaderDeprecatedConverter::_parse_struct() {
	DEV_ASSERT(get_pos() && get_pos()->get().type == TT::TK_STRUCT);
	TokenE *struct_start = get_pos();
	TokenE *struct_name_pos = get_next_token();
	EOF_FAIL(struct_name_pos);
	if (struct_name_pos->get().type == TT::TK_CURLY_BRACKET_OPEN) {
		return false; // No anonymous structs.
	}
	TokenE *struct_body_start = get_next_token();
	EOF_FAIL(struct_body_start);
	COND_LINE_MSG_FAIL(struct_body_start->get().type != TT::TK_CURLY_BRACKET_OPEN, struct_body_start->get().line, RTR("Expected '{' after struct declaration"));
	TokenE *struct_body_end = _get_end_of_closure();
	CLOSURE_FAIL(struct_body_end);
	COND_LINE_MSG_FAIL(struct_body_end->get().type != TT::TK_CURLY_BRACKET_CLOSE, struct_body_start->get().line, RTR("Expected '}' bracket at end of struct declaration"));

	COND_LINE_MSG_FAIL(!tokentype_is_identifier(struct_name_pos->get().type), struct_name_pos->get().line, RTR("Expected identifier after 'struct'"));
	String struct_name = get_token_literal_text(struct_name_pos->get());
	StructDecl struct_decl;
	struct_decl.start_pos = struct_start;
	struct_decl.name_pos = struct_name_pos;
	struct_decl.body_start_pos = struct_body_start;
	struct_decl.body_end_pos = struct_body_end;
	struct_decls[struct_name] = struct_decl;
	String struct_scope = "struct." + struct_name;
	for (TokenE *tk = struct_body_start; tk != struct_body_end; tk = get_next_token()) {
		if (!_process_decl_if_exist(struct_scope, false)) {
			return false;
		}
	}

	reset_to(struct_body_end);
	return true;
}

// Past the start and type tokens, at the id or bracket open token.
bool ShaderDeprecatedConverter::_process_decl_statement(TokenE *p_start_tok, TokenE *p_type_tok, const String &p_scope, bool p_func_args) {
	while (true) {
		EOF_FAIL(p_start_tok);
		EOF_FAIL(p_type_tok);
		COND_LINE_MSG_FAIL(!token_is_type(p_type_tok->get()), p_type_tok->get().line, RTR("Expected type in declaration"));
		TokenE *next_tk = get_pos();
		VarDecl var;
		var.start_pos = p_start_tok;
		var.type_pos = p_type_tok;
		var.is_func_arg = p_func_args;
		EOF_FAIL(next_tk);
		if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
			var.is_array = true;
			var.new_arr_style_decl = true;
			if (!_skip_array_size()) {
				return false;
			}
			next_tk = get_next_token();
			EOF_FAIL(next_tk);
		}
		COND_LINE_MSG_FAIL(!tokentype_is_identifier(next_tk->get().type), next_tk->get().line, RTR("Expected identifier after type in declaration"));
		var.name_pos = next_tk;
		String name = get_token_literal_text(var.name_pos->get());
		next_tk = get_next_token();
		EOF_FAIL(next_tk);
		TokenE *end_pos = next_tk;
		if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
			var.is_array = true;
			if (!_skip_array_size()) {
				return false;
			}
			end_pos = get_next_token();
			next_tk = end_pos;
			EOF_FAIL(next_tk);
		}
		if (next_tk->get().type == TT::TK_OP_ASSIGN) {
			end_pos = _get_end_of_closure();
			CLOSURE_FAIL(end_pos);
			reset_to(end_pos); // Skip the assignment.
			if (end_pos->get().type == TT::TK_PARENTHESIS_CLOSE && p_func_args) {
				next_tk = end_pos;
				end_pos = end_pos->prev(); // Including whitespace before parenthesis.
			} else {
				next_tk = get_next_token(); // comma or semi-colon
				EOF_FAIL(next_tk);
				end_pos = next_tk;
			}
		}
		var.end_pos = end_pos;
		COND_LINE_MSG_FAIL(p_func_args && !(next_tk->get().type == TT::TK_COMMA || next_tk->get().type == TT::TK_PARENTHESIS_CLOSE), next_tk->get().line, RTR("Expected ',' , or ')' after function argument declaration"));
		COND_LINE_MSG_FAIL(!p_func_args && !(next_tk->get().type == TT::TK_SEMICOLON || next_tk->get().type == TT::TK_COMMA), next_tk->get().line, RTR("Expected ',' or ';' after declaration"));
		if (!p_scope.begins_with("struct.")) {
			if (var_decls.has(name)) {
				var_decls[name].push_back(var);
			} else {
				var_decls[name] = { var };
			}
			if (!scope_declarations.has(p_scope)) {
				scope_declarations[p_scope] = HashSet<String>();
			}
			scope_declarations[p_scope].insert(name);
		} else {
			String struct_name = p_scope.substr(7);
			struct_decls[struct_name].members[name] = var;
		}
		if (next_tk->get().type == TT::TK_COMMA) {
			next_tk = get_next_token();
			EOF_FAIL(next_tk);
			p_start_tok = next_tk;
			if (p_func_args) {
				while (next_tk->get().type == TT::TK_CONST ||
						SL::is_token_precision(next_tk->get().type) ||
						SL::is_token_arg_qual(next_tk->get().type) ||
						SL::is_token_interpolation(next_tk->get().type)) {
					next_tk = get_next_token();
					EOF_FAIL(next_tk);
				}
				p_type_tok = next_tk; // next_tk is type
				COND_LINE_MSG_FAIL(!token_is_type(p_type_tok->get()), p_type_tok->get().line, RTR("Expected type after comma in function argument declaration"));
				next_tk = get_next_token(); // id
				EOF_FAIL(next_tk);
			} // otherwise, this is a compound declaration, leave type_tok as is
		} else if (next_tk->get().type == TT::TK_PARENTHESIS_CLOSE) {
			break;
		} else if (next_tk->get().type == TT::TK_SEMICOLON) {
			break;
		}
	}
	return true;
}

// Past the start and type tokens, at the id or bracket open token.
bool ShaderDeprecatedConverter::_process_func_decl_statement(TokenE *p_start_tok, TokenE *p_type_tok, bool p_first_pass) {
	FunctionDecl func;
	func.start_pos = p_start_tok; // type or const
	func.type_pos = p_type_tok; // type
	TokenE *next_tk = get_pos(); // id or array size
	if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
		func.has_array_return_type = true;
		if (!_skip_array_size()) {
			return false;
		}
		next_tk = get_next_token();
		EOF_FAIL(next_tk);
	}
	func.name_pos = next_tk; // id
	String name = get_token_literal_text(func.name_pos->get());
	func.args_start_pos = get_next_token(); // paren
	EOF_FAIL(func.args_start_pos);
	if (peek_next_tk_type() == TT::TK_PARENTHESIS_CLOSE) {
		func.args_end_pos = get_next_token();
		func.arg_count = 0;
	} else { // Args are present.
		func.args_end_pos = _get_end_of_closure();
		CLOSURE_FAIL(func.args_end_pos);
		COND_LINE_MSG_FAIL(func.args_end_pos->get().type != TT::TK_PARENTHESIS_CLOSE, func.args_end_pos->get().line, RTR("Expected ')' after function arguments"));
		if (!p_first_pass) { // first_pass == false means we've already parsed the args.
			// Skip the args.
			reset_to(func.args_end_pos);
		} else {
			TokenE *start_pos = get_next_token();
			TokenE *type_pos = start_pos;
			while (type_pos->get().type == TT::TK_CONST || SL::is_token_precision(type_pos->get().type) || SL::is_token_arg_qual(type_pos->get().type) || SL::is_token_interpolation(type_pos->get().type)) {
				type_pos = get_next_token();
				EOF_FAIL(type_pos);
			}
			COND_LINE_MSG_FAIL(!token_is_type(type_pos->get()), type_pos->get().line, RTR("Expected type in function argument declaration"));
			get_next_token(); // id or open bracket
			int var_count = var_decls.size();
			if (!_process_decl_statement(start_pos, type_pos, name, true)) {
				return false;
			}
			COND_LINE_MSG_FAIL(get_pos() != func.args_end_pos, get_pos()->get().line, RTR("Expected ')' after function arguments"));
			func.arg_count = var_decls.size() - var_count;
		}
	}
	// Currently at paren close.
	func.body_start_pos = get_next_token(); // Curly open.
	EOF_FAIL(func.body_start_pos);
	COND_LINE_MSG_FAIL(func.body_start_pos->get().type != TT::TK_CURLY_BRACKET_OPEN, func.body_start_pos->get().line, RTR("Expected '{' after function declaration"));
	func.body_end_pos = _get_end_of_closure();
	CLOSURE_FAIL(func.body_end_pos);
	COND_LINE_MSG_FAIL(func.body_end_pos->get().type != TT::TK_CURLY_BRACKET_CLOSE, func.body_start_pos->get().line, RTR("Expected '}' bracket"));
	if (p_first_pass) { // p_first_pass == false means the functions have already been processed.
		function_decls[name] = func;
#ifdef DEBUG_ENABLED
	} else {
		if (!function_decls.has(name)) {
			LINE_MSG_FAIL(func.start_pos->get().line, vformat(RTR("Function declaration not found in third pass (%s)"), name));
		} else {
			// Compare our values to ensure they match.
			FunctionDecl &first_pass = function_decls[name];
			// Don't check arg count, as it's not set in the second pass.
			bool matches = first_pass.start_pos == func.start_pos && first_pass.type_pos == func.type_pos && first_pass.name_pos == func.name_pos && first_pass.args_start_pos == func.args_start_pos && first_pass.args_end_pos == func.args_end_pos && first_pass.body_start_pos == func.body_start_pos && first_pass.body_end_pos == func.body_end_pos;
			COND_LINE_MSG_FAIL(!matches, func.start_pos->get().line, vformat(RTR("Function declaration mismatch in third pass (%s)"), name));
		}
#endif
	}
	return true;
}

bool ShaderDeprecatedConverter::_parse_decls(bool p_first_pass) {
	reset_to(after_shader_decl);
	String curr_func = "<global>";
	while (true) {
		TokenE *cur_tok = get_next_token();
		if (cur_tok->get().type == TT::TK_EOF) {
			break;
		}

		if (!p_first_pass) {
			for (KeyValue<String, FunctionDecl> &E : function_decls) {
				FunctionDecl &func = E.value;
				if (cur_tok == func.args_start_pos) {
					curr_func = E.key;
				} else if (cur_tok == func.body_end_pos) {
					curr_func = "<global>";
				}
			}
		}
		if (cur_tok->get().type == TT::TK_STRUCT) {
			if (!_skip_struct()) {
				return false;
			}
			continue;
		}
		if (cur_tok->get().type == TT::TK_UNIFORM) {
			if (!_skip_uniform()) {
				return false;
			}
			continue;
		}
		if (!_process_decl_if_exist(curr_func, p_first_pass)) {
			return false;
		}
	}
	return true;
}

bool ShaderDeprecatedConverter::_process_decl_if_exist(String p_curr_func, bool p_first_pass) {
	TokenE *cur_tok = get_pos();

	TokenE *start_pos = cur_tok;
	if (!_tok_is_start_of_decl(cur_tok->get())) {
		return true;
	}
	while (_tok_is_start_of_decl(cur_tok->get())) {
		if (token_is_type(cur_tok->get())) {
			break;
		}
		cur_tok = get_next_token();
		EOF_FAIL(cur_tok);
	}
	COND_LINE_MSG_FAIL(!token_is_type(cur_tok->get()), cur_tok->get().line, RTR("Expected type in declaration"));
	TokenE *type_pos = cur_tok;

	bool is_decl = tokentype_is_identifier(peek_next_tk_type());
	bool is_function = peek_next_tk_type(2) == TT::TK_PARENTHESIS_OPEN;
	if (!is_decl) {
		// Check if this is an array declaration.
		TokenE *next_tk = get_next_token();
		if (next_tk->get().type == TT::TK_BRACKET_OPEN) {
			if (!_skip_array_size()) {
				return true;
			}
			next_tk = get_pos();
			EOF_FAIL(next_tk);
			COND_LINE_MSG_FAIL(next_tk->get().type != TT::TK_BRACKET_CLOSE, next_tk->get().line, RTR("Expected ']' after array type"));
			TokenE *next_next_tk = get_next_token();
			if (next_next_tk && next_next_tk->get().type == TT::TK_IDENTIFIER) {
				is_decl = true;
				if (peek_next_tk_type() == TT::TK_PARENTHESIS_OPEN) {
					is_function = true;
				} else {
					is_function = false;
				}
			}
		}
		reset_to(cur_tok); // Backup to the Bracket open.
	}
	COND_LINE_MSG_FAIL(is_function && p_curr_func != "<global>", cur_tok->get().line, RTR("Unexpected function declaration"));
	if (!is_decl) {
		return true;
	}
	TokenE *id_tok = get_next_token(); // Id or bracket open.
	EOF_FAIL(id_tok);
	if (is_function) { // Function declaration.
		if (!_process_func_decl_statement(start_pos, type_pos, p_first_pass)) {
			return false;
		}
		// Backup to before the curly bracket open.
		get_prev_token();
	} else if (!p_first_pass) { // Other non-uniform declaration (global const, varying, locals, etc.).
		if (!_process_decl_statement(start_pos, type_pos, p_curr_func)) {
			return false;
		}
	}
	return true;
}

bool ShaderDeprecatedConverter::_parse_uniforms() {
	reset_to(after_shader_decl);
	while (true) {
		TokenE *cur_tok = get_next_token();
		if (cur_tok->get().type == TT::TK_EOF) {
			break;
		}
		switch (cur_tok->get().type) {
			case TT::TK_UNIFORM: {
				if (!_parse_uniform()) {
					return false;
				}
			} break;
			default:
				break;
		}
	}
	return true;
}

bool ShaderDeprecatedConverter::_parse_structs() {
	reset_to(after_shader_decl);
	while (true) {
		TokenE *cur_tok = get_next_token();
		if (cur_tok->get().type == TT::TK_EOF) {
			break;
		}
		switch (cur_tok->get().type) {
			case TT::TK_STRUCT: {
				if (!_parse_struct()) {
					return false;
				}
			} break;
			default:
				break;
		}
	}
	return true;
}

bool ShaderDeprecatedConverter::_preprocess_code() {
	COND_MSG_FAIL(code_tokens.size() == 0, RTR("Empty shader file"));
	StringName mode_string;
	{
		COND_MSG_FAIL(code_tokens.size() < 3, RTR("Invalid shader file"));
		TokenE *first_token = get_next_token();
		EOF_FAIL(first_token);
		COND_LINE_MSG_FAIL(first_token->get().type != TT::TK_SHADER_TYPE, first_token->get().line, RTR("Shader type must be first token"));
		TokenE *id_token = get_next_token();
		EOF_FAIL(id_token);
		COND_LINE_MSG_FAIL(id_token->get().type != TT::TK_IDENTIFIER, id_token->get().line, RTR("Invalid shader type"));
		mode_string = id_token->get().text;
		TokenE *token = get_next_token();
		EOF_FAIL(token);
		COND_LINE_MSG_FAIL(token->get().type != TT::TK_SEMICOLON, token->get().line, RTR("Expected semi-colon after shader type"));
		shader_mode = get_shader_mode_from_string(mode_string);
	}
	after_shader_decl = get_pos();

	/***
	 * The first pass gets the uniform declarations; we require this is to ensure idempotency for inserting new uniforms and replacing type hints.
	 * The second pass gets the function declarations; these are used for determining if a renamed built-in is valid in the current scope.
	 * The third pass gets the variable declarations; These are used for determining if renamed built-ins have been previously declared, and for detecting new keywords used as identifiers.
	 */
	// first pass, get uniform declarations.
	if (!_parse_uniforms()) {
		err_str = vformat(RTR("First pre-process pass failed: %s"), err_str);
		curr_ptr = code_tokens.front();
		return false;
	}
	if (!_parse_structs()) {
		err_str = vformat(RTR("First pre-process pass failed: %s"), err_str);
		curr_ptr = code_tokens.front();
		return false;
	}
	// Second pass, get function declarations.
	if (!_parse_decls(true)) {
		function_pass_failed = true;
		err_str = vformat(RTR("Second pre-process pass failed: %s"), err_str);
		curr_ptr = code_tokens.front();
		return false;
	}
	// Third pass, get variable declarations.
	if (!_parse_decls(false)) {
		var_pass_failed = true;
		err_str = vformat(RTR("Third pre-process pass failed: %s"), err_str);
		curr_ptr = code_tokens.front();
		return false;
	}
	curr_ptr = code_tokens.front();
	return true;
}

int ShaderDeprecatedConverter::get_error_line() const {
	return err_line;
}

bool ShaderDeprecatedConverter::is_code_deprecated(const String &p_code) {
	// Quick check to see if it's a shader file with a deprecated type.
	String mode_str = SL::get_shader_type(p_code);
	if (mode_str.is_empty()) {
		// If it failed, it's because it was prefixed with a preproc directive (4.x only) or it's not a shader file.
		return false;
	}
	RS::ShaderMode mode = get_shader_mode_from_string(mode_str);
	if (mode == RS::SHADER_MAX) {
		return false;
	}
	old_code = p_code;
	reset();
	if (_has_any_preprocessor_directives()) {
		return false;
	}
	if (!_preprocess_code()) {
		return false;
	}
	return _is_code_deprecated();
}

bool ShaderDeprecatedConverter::_has_any_preprocessor_directives() {
	TokenE *cur_tok = code_tokens.front();
	while (cur_tok) {
		if (cur_tok->get().type == TT::TK_PREPROC_DIRECTIVE) {
			return true;
		}
		cur_tok = cur_tok->next();
	}
	return false;
}

bool ShaderDeprecatedConverter::_is_code_deprecated() {
	reset_to(after_shader_decl);

	// Negative cases first, then positive cases.
	bool is_3x = false;

	// Check declarations for negative cases.
	for (const KeyValue<String, UniformDecl> &E : uniform_decls) {
		const UniformDecl &uni = E.value;
		if (uni.is_array) { // 3.x did not have array uniforms.
			return false;
		} else if (tokentype_is_new_type(uni.type_pos->get().type)) { // Usage of new type.
			return false;
		} else if (uni.has_uniform_qual()) { // 3.x did not have uniform qualifiers.
			return false;
		}
		for (const TokenE *hint : uni.hint_poses) {
			if (tokentype_is_new_hint(hint->get().type)) { // Usage of new hint.
				return false;
			}
		}
	}

	for (const KeyValue<String, FunctionDecl> &E : function_decls) {
		const FunctionDecl &func = E.value;
		if (func.has_array_return_type) { // 3.x did not have array return types.
			return false;
		} else if (tokentype_is_new_type(func.type_pos->get().type) && !struct_decls.has(get_tokentype_text(func.type_pos->get().type))) { // Usage of new type.
			return false;
		} else if (func.is_new_main_function(shader_mode)) { // Has the process function with the same signature.
			return false;
		}
	}

	for (const KeyValue<String, StructDecl> &E : struct_decls) {
		const StructDecl &str = E.value;
		for (const KeyValue<String, VarDecl> &v : str.members) {
			if (tokentype_is_new_type(v.value.type_pos->get().type) && !struct_decls.has(get_tokentype_text(v.value.type_pos->get().type))) { // Usage of new type.
				return false;
			}
		}
	}

	for (const KeyValue<String, Vector<VarDecl>> &E : var_decls) {
		for (const VarDecl &var_decl : E.value) {
			if (var_decl.is_array && var_decl.is_func_arg) { // 3.x did not allow array function arguments.
				return false;
			} else if (var_decl.new_arr_style_decl) { // 3.x did not have the `float[] x` style of array declarations for non-struct members.
				return false;
			} else if (tokentype_is_new_type(var_decl.type_pos->get().type) && !struct_decls.has(get_tokentype_text(var_decl.type_pos->get().type))) { // Usage of new type.
				return false;
			}
		}
	}

	// Check token stream for negative cases.
	{
		reset_to(after_shader_decl);
		String curr_func = "<global>";
		while (true) {
			TokenE *cur_tok = get_next_token();
			DEV_ASSERT(cur_tok);
			if (cur_tok->get().type == TT::TK_EOF || cur_tok->get().type == TT::TK_ERROR) {
				break;
			}
			for (KeyValue<String, FunctionDecl> &E : function_decls) {
				FunctionDecl &func = E.value;
				if (cur_tok == func.args_start_pos) {
					curr_func = E.key;
					break;
				} else if (cur_tok == func.body_end_pos) {
					curr_func = "<global>";
					break;
				}
			}
			if (cur_tok->get().type == TT::TK_STRUCT) {
				if (!_skip_struct()) {
					return false;
				}
				continue;
			}
			String id = get_token_literal_text(cur_tok->get());
			if (cur_tok->get().type == TT::TK_IDENTIFIER) {
				if (has_builtin_rename(shader_mode, id, curr_func) || is_removed_builtin(shader_mode, id, curr_func)) {
					if (scope_has_decl(curr_func, id) || function_decls.has(id)) {
						// The renamed built-ins are global identifiers in 3.x and can't be redefined in either the global scope or the function scope they're valid for.
						// If they were declared previously within the global or current scope, this would be a 4.x shader.
						return false;
					}
				} else if (id_is_new_builtin_func(id) && peek_next_tk_type() == TT::TK_PARENTHESIS_OPEN && !function_decls.has(id)) { // Use of a new built-in function without a corresponding declaration.
					return false;
				}
			}
		}
	}

	// Positive cases.

	// Check declarations for positive cases.
	for (const KeyValue<String, UniformDecl> &E : uniform_decls) {
		const UniformDecl &uni = E.value;
		if (uni.type_pos->get().type == TT::TK_IDENTIFIER && has_removed_type(get_token_literal_text(uni.type_pos->get()))) { // Unported 3.x type.
			return true;
		} else if (tokentype_is_new_reserved_keyword(uni.name_pos->get().type)) { // Uniform name is a new reserved keyword.
			return true;
		} else if (uni.has_interp_qual()) { // Newer versions of Godot disallow interpolation qualifiers for uniforms.
			return true;
		}
		for (const TokenE *hint : uni.hint_poses) {
			if (hint->get().type == TT::TK_IDENTIFIER && has_hint_replacement(get_token_literal_text(hint->get()))) {
				return true;
			}
		}
	}

	for (const KeyValue<String, StructDecl> &E : struct_decls) {
		const StructDecl &struct_decl = E.value;
		if (tokentype_is_new_reserved_keyword(struct_decl.name_pos->get().type)) { // Struct identifier is new reserved keyword.
			return true;
		}
		for (const KeyValue<String, VarDecl> &v : struct_decl.members) {
			if (v.value.type_pos->get().type == TT::TK_IDENTIFIER && has_removed_type(get_token_literal_text(v.value.type_pos->get()))) { // Unported 3.x type.
				return true;
			} else if (tokentype_is_new_reserved_keyword(v.value.name_pos->get().type)) { // Struct member identifier is new reserved keyword.
				return true;
			}
		}
	}

	for (const KeyValue<String, FunctionDecl> &E : function_decls) {
		const FunctionDecl &func = E.value;
		String name = get_token_literal_text(func.name_pos->get());
		String type_name = get_token_literal_text(func.type_pos->get());
		if (func.type_pos->get().type == TT::TK_IDENTIFIER && has_removed_type(type_name)) { // Unported 3.x type.
			return true;
		} else if (func.is_renamed_main_function(shader_mode)) { // Matching renamed function.
			return true;
		} else if (tokentype_is_new_reserved_keyword(func.name_pos->get().type)) { // Function identifier is new reserved keyword.
			return true;
		} else if (id_is_new_builtin_func(name)) { // Declaration of function with the same name as a new built-in function.
			return true;
		}
	}

	for (const KeyValue<String, Vector<VarDecl>> &E : var_decls) {
		for (const VarDecl &var_decl : E.value) {
			if (var_decl.type_pos->get().type == TT::TK_IDENTIFIER && has_removed_type(get_token_literal_text(var_decl.type_pos->get()))) { // Unported 3.x type.
				return true;
			} else if (tokentype_is_new_reserved_keyword(var_decl.name_pos->get().type)) { // Id is new reserved keyword.
				return true;
			}
		}
	}

	String curr_func = "<global>";
	reset_to(after_shader_decl);
	// Check token stream for positive cases.
	while (true) {
		TokenE *cur_tok = get_next_token();
		if (cur_tok->get().type == TT::TK_EOF || cur_tok->get().type == TT::TK_ERROR) {
			break;
		}

		for (KeyValue<String, FunctionDecl> &E : function_decls) {
			FunctionDecl &func = E.value;
			if (cur_tok == func.body_start_pos) {
				curr_func = E.key;
				break;
			} else if (cur_tok == func.body_end_pos) {
				curr_func = "<global>";
				break;
			}
		}
		if (cur_tok->get().type == TT::TK_STRUCT) {
			if (!_skip_struct()) {
				return false;
			}
			continue;
		}

		switch (cur_tok->get().type) {
			case TT::TK_FLOAT_CONSTANT: {
				String const_str = get_token_literal_text(cur_tok->get()).to_lower();
				// 3.x float constants allowed for a value without a decimal point if it ended in `f` (e.g. `1f`).
				if (const_str.ends_with("f") && const_str.find(".") == -1 && const_str.find("e") == -1) {
					return true;
				}
			} break;
			case TT::TK_RENDER_MODE: {
				while (true) {
					TokenE *next_tk = get_next_token();
					if (next_tk->get().type == TT::TK_IDENTIFIER) {
						String id_text = get_token_literal_text(next_tk->get());
						if (is_renamed_render_mode(shader_mode, id_text) || has_removed_render_mode(shader_mode, id_text)) {
							return true;
						}
					} else {
						COND_LINE_MSG_FAIL(next_tk->get().type != TT::TK_COMMA && next_tk->get().type != TT::TK_SEMICOLON, next_tk->get().line, "Invalid render mode declaration");
					}
					if (next_tk->get().type == TT::TK_SEMICOLON) {
						break;
					}
				}
			} break;
			case TT::TK_IDENTIFIER: {
				String id = get_token_literal_text(cur_tok->get());
				if (has_builtin_rename(shader_mode, id, curr_func) || is_removed_builtin(shader_mode, id, curr_func)) {
					if ((!scope_has_decl(curr_func, id) && !function_decls.has(id))) {
						return true;
					}
				} else if (has_removed_type(id) && peek_next_tk_type() == TT::TK_IDENTIFIER) {
					// Declaration with unported 3.x type.
					return true;
				}
			} break;
			default:
				break;
		}
	}
	return is_3x;
}

String ShaderDeprecatedConverter::get_error_text() const {
	return err_str;
}

bool ShaderDeprecatedConverter::_check_deprecated_type(TokenE *p_type_pos) {
	if (p_type_pos->get().type == TT::TK_IDENTIFIER && has_removed_type(get_token_literal_text(p_type_pos->get()))) {
		const String i_err_msg = vformat(RTR("Deprecated type '%s' is not supported by this version of Godot."), get_token_literal_text(p_type_pos->get()));
		COND_LINE_MSG_FAIL(fail_on_unported, p_type_pos->get().line, i_err_msg);
		_add_comment_before(i_err_msg, p_type_pos);
	}
	return true;
}

ShaderDeprecatedConverter::TokenE *ShaderDeprecatedConverter::_rename_keyword_id(TokenE *pos, bool p_detected_3x, const HashMap<TokenType, String> &p_new_reserved_word_renames) {
	String rename = p_new_reserved_word_renames[pos->get().type];
	reset_to(pos);
	return replace_curr(mkTok(TT::TK_IDENTIFIER, rename), "Identifier '%s' is a reserved word in this version of Godot, renamed to '%s'.");
}

bool ShaderDeprecatedConverter::_handle_new_keyword_rename(TokenE *pos, bool p_detected_3x, HashMap<TokenType, String> &p_new_reserved_word_renames) {
	TokenType tk_type = pos->get().type;
	String p_name = get_token_literal_text(pos->get());
	if (tokentype_is_new_reserved_keyword(tk_type)) {
		if (!p_detected_3x) {
			// If we're not sure it's a 3.x shader, just add a comment.
			_add_comment_before(vformat(RTR("Identifier '%s' is a reserved word in this version of Godot."), p_name), pos);
			return false;
		}
		if (!p_new_reserved_word_renames.has(tk_type)) {
			String rename = p_name + String("_");
			while (function_decls.has(rename) || uniform_decls.has(rename) || var_decls.has(rename) || struct_decls.has(rename)) {
				rename += "_";
			}
			p_new_reserved_word_renames[tk_type] = rename;
		}
		return true;
	}
	return false;
}

bool ShaderDeprecatedConverter::convert_code(const String &p_code) {
	/**
	 * We need to do the following:
	 *  * Replace everything in RenamesMap3To4::shaders_renames
	 *	* the usage of SCREEN_TEXTURE, DEPTH_TEXTURE, and NORMAL_ROUGHNESS_TEXTURE necessitates adding a uniform declaration at the top of the file
	 *	* async_visible and async_hidden render modes need to be removed
	 *	* If shader_type is "particles", need to rename the function "void vertex()" to "void process()"
	 *  * Invert all usages of CLEARCOAT_GLOSS:
	 *    * Invert all lefthand assignments:
	 * 			- `CLEARCOAT_GLOSS = 5.0 / foo;`
	 * 			becomes: `CLEARCOAT_ROUGHNESS = (1.0 - (5.0 / foo));`,
	 *          - `CLEARCOAT_GLOSS *= 1.1;`
	 * 			becomes `CLEARCOAT_ROUGHNESS = (1.0 - ((1.0 - CLEARCOAT_ROUGHNESS) * 1.1));`
	 *    * Invert all righthand usages
	 * 			- `foo = CLEARCOAT_GLOSS;`
	 * 			becomes: `foo = (1.0 - CLEARCOAT_ROUGHNESS);`
	 *  * Wrap `INDEX` in `int()` casts if necessary.
	 *	* Check for use of `specular_blinn` and `specular_phong` render modes; not supported in 4.x, throw an error.
	 *	* Check for use of `MODULATE`; not supported in 4.x, throw an error.
	 *	* Check for use of new reserved keywords as identifiers; rename them if necessary.
	 *  * Check for use of new built-in functions with a corresponding declaration; rename them if necessary.
	 */
	old_code = p_code;
	reset();
	if (_has_any_preprocessor_directives()) { // We refuse to process any shader with preprocessor directives, as they're not supported in 3.x and they make our parsing assumptions invalid.
		err_str = RTR("Cannot convert new shader with pre-processor directives.");
		return false;
	}
	if (!_preprocess_code()) {
		return false;
	}
	bool detected_3x = _is_code_deprecated(); // Calls preprocess_code().
	if (!detected_3x && err_str != "") {
		return false;
	} // We don't fail if the code is detected as not deprecated, as the user may have forced it; instead, we just avoid doing the more dicey replacements, like renaming new keywords.
	COND_MSG_FAIL(shader_mode == RS::SHADER_MAX, RTR("Detected Shader type is not a 3.x type.")); // However, we do fail if it's a new shader type, because we don't do any replacements for those.
	err_str = "";
	curr_ptr = after_shader_decl;

	// Renaming changed hints.
	Vector<TokenE *> all_hints;
	for (KeyValue<String, UniformDecl> &E : uniform_decls) {
		UniformDecl &uni = E.value;
		if (uni.has_interp_qual()) { // Removing interpolation qualifiers before the type name, which was allowed in 3.x.
			reset_to(uni.interp_qual_pos);
			_add_comment_before(vformat(RTR("Interpolation qualifiers not supported in this version of Godot, '%s' removed."), get_token_literal_text(uni.interp_qual_pos->get())), uni.start_pos, false);
			remove_cur_and_get_next();
			uni.interp_qual_pos = nullptr;
			reset_to(after_shader_decl);
		}
		String name = get_token_literal_text(uni.name_pos->get());
		for (int i = 0; i < uni.hint_poses.size(); i++) {
			TokenE *hint = uni.hint_poses[i];
			String hint_name = get_token_literal_text(hint->get());
			if (hint->get().type == TT::TK_IDENTIFIER && has_hint_replacement(hint_name)) {
				// replace the hint
				reset_to(hint);
				TT hint_rename = get_hint_replacement(hint_name);
				hint = replace_curr(mkTok(hint_rename), "Hint '%s' renamed to '%s'.");
				uni.hint_poses.write[i] = hint;
				reset_to(after_shader_decl);
			}
			all_hints.push_back(hint);
		}
	}

	// Renaming new reserved keywords used as identifiers (e.g "global", "instance").
	// To ensure idempotency, we only do this if we know for certain that the new keyword was used in a declaration.
	HashMap<TokenType, String> new_reserved_word_renames;
	HashMap<String, String> func_renames;
	HashMap<String, String> struct_renames;
	HashMap<String, String> struct_member_renames;
	HashMap<String, String> nonfunc_globals_renames; // Only used if a function is renamed and an existing global conflicts with the rename.

	for (KeyValue<String, UniformDecl> &E : uniform_decls) {
		UniformDecl &uni = E.value;
		if (!_check_deprecated_type(uni.type_pos)) {
			return false;
		}

		if (_handle_new_keyword_rename(uni.name_pos, detected_3x, new_reserved_word_renames)) {
			uni.name_pos = _rename_keyword_id(uni.name_pos, detected_3x, new_reserved_word_renames);
			reset_to(after_shader_decl);
		}
	}

	for (KeyValue<String, StructDecl> &E : struct_decls) {
		StructDecl &struct_decl = E.value;
		if (_handle_new_keyword_rename(struct_decl.name_pos, detected_3x, new_reserved_word_renames)) {
			struct_decl.name_pos = _rename_keyword_id(struct_decl.name_pos, detected_3x, new_reserved_word_renames);
			struct_renames[E.key] = get_token_literal_text(struct_decl.name_pos->get());
			reset_to(after_shader_decl);
		}
		for (KeyValue<String, VarDecl> &M : struct_decl.members) {
			VarDecl &var = M.value;
			if (!_check_deprecated_type(var.type_pos)) {
				return false;
			}
			String type = get_token_literal_text(var.type_pos->get());
			if (_handle_new_keyword_rename(var.name_pos, detected_3x, new_reserved_word_renames)) {
				var.name_pos = _rename_keyword_id(var.name_pos, detected_3x, new_reserved_word_renames);
				struct_member_renames[M.key] = get_token_literal_text(var.name_pos->get());
				reset_to(after_shader_decl);
			}
			if (struct_renames.has(type)) {
				reset_to(var.type_pos);
				var.type_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, struct_renames[type]), "Struct type '%s' renamed to '%s'.");
				reset_to(after_shader_decl);
			}
		}
	}
	for (KeyValue<String, Vector<VarDecl>> &E : var_decls) {
		if (E.value.is_empty()) {
			continue;
		}
		// Check for deprecated type.
		for (VarDecl &var : E.value) {
			if (!_check_deprecated_type(var.type_pos)) {
				return false;
			}
			String type = get_token_literal_text(var.type_pos->get());
			if (struct_renames.has(type)) {
				reset_to(var.type_pos);
				var.type_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, struct_renames[type]), "Struct type '%s' renamed to '%s'.");
				reset_to(after_shader_decl);
			}
		}

		if (_handle_new_keyword_rename(E.value[0].name_pos, detected_3x, new_reserved_word_renames)) {
			for (VarDecl &varDecl : E.value) {
				// replace the identifier
				reset_to(varDecl.name_pos);
				if (varDecl.name_pos == varDecl.start_pos) {
					varDecl.name_pos = _rename_keyword_id(varDecl.name_pos, detected_3x, new_reserved_word_renames);
					varDecl.start_pos = varDecl.name_pos;
				} else {
					varDecl.name_pos = _rename_keyword_id(varDecl.name_pos, detected_3x, new_reserved_word_renames);
				}
				reset_to(after_shader_decl);
			}
		}
	}
	bool has_new_main_function = false;
	Vector<String> new_main_function_names;
	for (KeyValue<String, FunctionDecl> &E : function_decls) {
		if (E.value.is_new_main_function(shader_mode)) {
			has_new_main_function = true;
			break;
		}
	}
	static const char *conflict_comment_prefix = "Renamed %s to avoid conflict with new main function, %s.";
	for (KeyValue<String, FunctionDecl> &E : function_decls) {
		FunctionDecl &var = E.value;
		if (!_check_deprecated_type(var.type_pos)) {
			return false;
		}
		String type = get_token_literal_text(var.type_pos->get());
		if (struct_renames.has(type)) {
			reset_to(var.type_pos);
			var.type_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, struct_renames[type]), "Struct type '%s' renamed to '%s'.");
			reset_to(after_shader_decl);
		}
		String name = get_token_literal_text(var.name_pos->get());
		if (!has_new_main_function && var.is_renamed_main_function(shader_mode)) {
			// replace the function name
			String rename = get_main_function_rename(name);
			reset_to(var.name_pos);
			var.name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, rename), "Function '%s' renamed to '%s' in this version of Godot.");
			reset_to(after_shader_decl);
			func_renames[name] = rename;
			// Only doing this because "process" is a common word and we don't want to clobber an existing function/global named that.
			// Won't clobber a pre-exising "process" function that has the correct main signature because of the check before.
			if (function_decls.has(rename) || scope_has_decl("<global>", rename)) {
				String rerename = rename + String("_");
				while (function_decls.has(rerename) || uniform_decls.has(rerename) || var_decls.has(rerename)) {
					rerename += "_";
				}
				if (function_decls.has(rename)) {
					func_renames[rename] = rerename;
					FunctionDecl &rere_func = function_decls[rename];
					reset_to(rere_func.name_pos);
					rere_func.name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, rerename), conflict_comment_prefix);
					reset_to(after_shader_decl);
				} else if (uniform_decls.has(rename)) {
					nonfunc_globals_renames[rename] = rerename;
					UniformDecl &rere_uni = uniform_decls[rename];
					reset_to(rere_uni.name_pos);
					rere_uni.name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, rerename), conflict_comment_prefix);
					reset_to(after_shader_decl);
				} else if (var_decls.has(rename) && scope_declarations.has("<global>") && scope_declarations["<global>"].has(rename)) {
					nonfunc_globals_renames[rename] = rerename;
					for (int i = 0; i < var_decls[rename].size(); i++) {
						VarDecl &rere_var = var_decls[rename].write[i];
						reset_to(rere_var.name_pos);
						rere_var.name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, rerename), conflict_comment_prefix);
						reset_to(after_shader_decl);
					}
				}
			}
		} else if (id_is_new_builtin_func(name)) {
			String rename = name + String("_");
			while (function_decls.has(rename) || uniform_decls.has(rename) || var_decls.has(rename)) {
				rename += "_";
			}
			func_renames[name] = rename;
			// replace the function name
			reset_to(var.name_pos);
			var.name_pos = replace_curr(mkTok(TT::TK_IDENTIFIER, rename), "Function '%s' is a built-in function in this version of Godot, renamed to '%s'.");
			reset_to(after_shader_decl);
		} else if (_handle_new_keyword_rename(var.name_pos, detected_3x, new_reserved_word_renames)) {
			var.name_pos = _rename_keyword_id(var.name_pos, detected_3x, new_reserved_word_renames);
			reset_to(after_shader_decl);
		}
	}
	bool in_function = false;
	String curr_func = "<global>";
	reset_to(after_shader_decl);
	static Vector<String> uniform_qualifiers = { "global", "instance" };
	while (true) {
		TokenE *cur_tok = get_next_token();
		if (cur_tok->get().type == TT::TK_EOF) {
			break;
		}
		for (KeyValue<String, FunctionDecl> &E : function_decls) {
			FunctionDecl &func = E.value;
			if (cur_tok == func.args_start_pos) {
				in_function = true;
				curr_func = E.key; // The key is the ORIGINAL function name, not the potentially renamed one.
			} else if (in_function && cur_tok == func.body_end_pos) {
				in_function = false;
				curr_func = "<global>";
			}
		}
		if (cur_tok->get().type == TT::TK_STRUCT) {
			if (!_skip_struct()) {
				return false;
			}
			continue;
		}
		String cur_tok_text = get_token_literal_text(cur_tok->get());
		if (cur_tok->get().pos != NEW_IDENT && new_reserved_word_renames.has(cur_tok->get().type)) {
			if (peek_prev_tk_type() == TT::TK_PERIOD && struct_member_renames.has(cur_tok_text)) {
				cur_tok = _rename_keyword_id(cur_tok, detected_3x, new_reserved_word_renames);
				continue;
			}
			if (!(scope_has_decl(curr_func, cur_tok_text) || (function_decls.has(cur_tok_text) && peek_next_tk_type() == TT::TK_PARENTHESIS_OPEN))) {
				continue; // Don't replace if this rename is not in the current scope or is not a renamed function call.
			}
			// Just extra insurance against replacing legit new keywords.
			if (uniform_qualifiers.has(cur_tok_text)) {
				if (peek_next_tk_type() == TT::TK_UNIFORM) {
					continue; // Don't replace uniform qualifiers.
				}
			} else if (all_hints.has(cur_tok)) {
				continue; // Hint, don't replace it.
			}
			cur_tok = _rename_keyword_id(cur_tok, detected_3x, new_reserved_word_renames);
			continue;
		}
		switch (cur_tok->get().type) {
			case TT::TK_FLOAT_CONSTANT: {
				// Earlier versions of Godot 3.x (< 3.5) allowed the use of the `f` sigil with float constants without a decimal place.
				if (cur_tok_text.ends_with("f") && !cur_tok_text.contains(".") && !cur_tok_text.contains("e")) {
					cur_tok_text = cur_tok_text.substr(0, cur_tok_text.length() - 1) + ".0f";
					_add_comment_before(RTR("Float constant without decimal point requires a trailing '.0' in this version of Godot."), cur_tok, false);
					cur_tok = replace_curr(mkTok(TT::TK_FLOAT_CONSTANT, cur_tok_text, 0xdeadbeef));
				}
			} break;
			case TT::TK_RENDER_MODE: {
				// we only care about the ones for spatial
				if (shader_mode == RenderingServer::ShaderMode::SHADER_SPATIAL) {
					while (true) {
						TokenE *next_tk = get_next_token();
						if (next_tk->get().type == TT::TK_IDENTIFIER) {
							String id_text = get_token_literal_text(next_tk->get());
							if (has_removed_render_mode(shader_mode, id_text)) {
								String comment = "Deprecated render mode '%s' is not supported by this version of Godot.";
								if (!can_remove_render_mode(id_text)) {
									const String i_err_msg = vformat(RTR(comment), id_text);
									COND_LINE_MSG_FAIL(fail_on_unported, next_tk->get().line, i_err_msg);
									_add_comment_before(i_err_msg, next_tk);
								} else {
									comment = comment.substr(0, comment.length() - 1) + ", removed.";
									_add_comment_before(vformat(RTR(comment), id_text), next_tk, true);
									if (peek_next_tk_type() == TT::TK_COMMA) {
										TokenE *comma = get_next_token();
										reset_to(next_tk); // Reset to the identifier.
										EOF_FAIL(comma->next());
										next_tk = _remove_from_curr_to(comma->next()); // Inclusive of comma.
									} else if (peek_prev_tk_type() == TT::TK_COMMA && peek_next_tk_type() == TT::TK_SEMICOLON) {
										TokenE *end = get_next_token();
										reset_to(next_tk); // Back to identifier.
										next_tk = get_prev_token(); // comma
										next_tk = _remove_from_curr_to(end); // Exclusive of semi-colon.
										break; // We're at the end of the render_mode declaration.
									} else if (peek_prev_tk_type() == TT::TK_RENDER_MODE && peek_next_tk_type() == TT::TK_SEMICOLON) {
										// Remove the whole line.
										TokenE *semi = get_next_token();
										COND_LINE_MSG_FAIL(!semi->next(), semi->get().line, "Unexpected EOF???"); // We should always have an EOF token at the end of the stream.
										reset_to(next_tk); // Back to identifier.
										next_tk = get_prev_token(); // render_mode
										next_tk = _remove_from_curr_to(semi->next()); // Inclusive of semi-colon.
										break;
									} else {
										// we shouldn't be here
										LINE_MSG_FAIL(next_tk->get().line, RTR("Unexpected token after render mode declaration."));
									}
								}
							} else if (is_renamed_render_mode(shader_mode, id_text)) {
								next_tk = replace_curr(mkTok(TT::TK_IDENTIFIER, get_render_mode_rename(id_text)), "Render mode ");
							}
						} else {
							COND_LINE_MSG_FAIL(next_tk->get().type != TT::TK_COMMA && next_tk->get().type != TT::TK_SEMICOLON, next_tk->get().line, RTR("Expected ',' or ';' after render mode declaration."));
						}
						if (next_tk->get().type == TT::TK_SEMICOLON) {
							break;
						}
					}
				}
			} break;
			case TT::TK_IDENTIFIER: {
				if (cur_tok->get().pos == NEW_IDENT) { // Skip already-replaced identifiers.
					break;
				}
				if (peek_prev_tk_type() == TT::TK_PERIOD) {
					break; // Struct member access, don't replace it.
				}
				if (func_renames.has(cur_tok_text) && peek_next_tk_type() == TT::TK_PARENTHESIS_OPEN) { // Function call.
					cur_tok = replace_curr(mkTok(TT::TK_IDENTIFIER, func_renames[cur_tok_text]), "Function call ");
				} else if (nonfunc_globals_renames.has(cur_tok_text) && peek_next_tk_type() != TT::TK_PARENTHESIS_OPEN) {
					cur_tok = replace_curr(mkTok(TT::TK_IDENTIFIER, nonfunc_globals_renames[cur_tok_text]), conflict_comment_prefix);
				} else if (is_removed_builtin(shader_mode, cur_tok_text, curr_func) && !scope_has_decl(curr_func, cur_tok_text)) {
					if (get_removed_builtin_uniform_type(cur_tok_text) == TT::TK_ERROR) {
						const String i_err_str = vformat(RTR("Deprecated built-in '%s' is not supported by this version of Godot"), cur_tok_text);
						COND_LINE_MSG_FAIL(fail_on_unported, cur_tok->get().line, i_err_str);
						_add_comment_before(i_err_str, cur_tok);
					}
					COND_LINE_MSG_FAIL(!_insert_uniform_declaration(cur_tok_text), cur_tok->get().line, RTR("Failed to insert uniform declaration"));
				} else if (cur_tok_text == "INDEX" && has_builtin_rename(shader_mode, cur_tok_text, curr_func) && !scope_has_decl(curr_func, cur_tok_text)) {
					// INDEX was an int in 3.x, but is a uint in later versions.
					// Need to wrap it in a `int()` cast.
					// This is safe because this will only trigger if the `particles` function is "vertex" (which is renamed to "process").

					// Don't do this if it's already wrapped in a int(), uint() or float().
					if (peek_prev_tk_type() == TT::TK_PARENTHESIS_OPEN && peek_next_tk_type() == TT::TK_PARENTHESIS_CLOSE) {
						TT peeked_type = peek_prev_tk_type(2);
						if (peeked_type == TT::TK_TYPE_INT || peeked_type == TT::TK_TYPE_UINT || peeked_type == TT::TK_TYPE_FLOAT) {
							break;
						}
					}
					_add_comment_before(vformat(RTR("INDEX is uint in this version of Godot, wrapped INDEX in 'int()' cast.")), cur_tok, false);
					insert_before({ mkTok(TT::TK_TYPE_INT), mkTok(TT::TK_PARENTHESIS_OPEN) }, cur_tok);
					insert_after(mkTok(TT::TK_PARENTHESIS_CLOSE), cur_tok);
				} else if (cur_tok_text == "CLEARCOAT_GLOSS" && has_builtin_rename(shader_mode, cur_tok_text, curr_func) && !scope_has_decl(curr_func, cur_tok_text)) {
					cur_tok = replace_curr(mkTok(TT::TK_IDENTIFIER, "CLEARCOAT_ROUGHNESS"), "Inverting usage of '%s' to '%s'.");
					List<Token>::Element *assign_closure_end = nullptr;
					switch (peek_next_tk_type()) {
						case TT::TK_OP_ASSIGN:
						case TT::TK_OP_ASSIGN_ADD:
						case TT::TK_OP_ASSIGN_SUB:
						case TT::TK_OP_ASSIGN_MUL:
						case TT::TK_OP_ASSIGN_DIV: {
							assign_closure_end = _get_end_of_closure();
							CLOSURE_FAIL(assign_closure_end);

							TokenE *assign_tk = get_next_token();
							TokenE *insert_pos = assign_tk;
							if (assign_tk->next() && assign_tk->next()->get().type == TT::TK_SPACE) {
								insert_pos = assign_tk->next();
							}
							// " = (1.0 - ("
							Vector<Token> assign_prefix = {
								mkTok(TT::TK_OP_ASSIGN),
								mkTok(TT::TK_SPACE),
								mkTok(TT::TK_PARENTHESIS_OPEN),
								mkTok(TT::TK_FLOAT_CONSTANT, {}, 1.0),
								mkTok(TT::TK_SPACE),
								mkTok(TT::TK_OP_SUB),
								mkTok(TT::TK_SPACE),
								mkTok(TT::TK_PARENTHESIS_OPEN),
							};
							if (assign_tk->get().type != TT::TK_OP_ASSIGN) {
								// " = (1.0 - ((1.0 - CLEARCOAT_ROUGHNESS) {op}
								assign_prefix.append_array(
										{ mkTok(TT::TK_PARENTHESIS_OPEN),
												mkTok(TT::TK_FLOAT_CONSTANT, {}, 1.0),
												mkTok(TT::TK_SPACE),
												mkTok(TT::TK_OP_SUB),
												mkTok(TT::TK_SPACE),
												mkTok(TT::TK_IDENTIFIER, "CLEARCOAT_ROUGHNESS"),
												mkTok(TT::TK_PARENTHESIS_CLOSE),
												mkTok(TT::TK_SPACE) });
							}
							switch (assign_tk->get().type) {
								case TT::TK_OP_ASSIGN_ADD: {
									assign_prefix.append_array({ mkTok(TT::TK_OP_ADD), mkTok(TT::TK_SPACE) });
								} break;
								case TT::TK_OP_ASSIGN_SUB: {
									assign_prefix.append_array({ mkTok(TT::TK_OP_SUB), mkTok(TT::TK_SPACE) });
								} break;
								case TT::TK_OP_ASSIGN_MUL: {
									assign_prefix.append_array({ mkTok(TT::TK_OP_MUL), mkTok(TT::TK_SPACE) });
								} break;
								case TT::TK_OP_ASSIGN_DIV: {
									assign_prefix.append_array({ mkTok(TT::TK_OP_DIV), mkTok(TT::TK_SPACE) });
								} break;
								default:
									break;
							}
							insert_after(assign_prefix, insert_pos);

							// remove the assignment token
							if (assign_tk != insert_pos && insert_pos->next()) {
								// remove the extraneous space too if necessary
								_remove_from_curr_to(insert_pos->next()); // Exclusive of the token after the space
							} else {
								remove_cur_and_get_next();
							}
							// "))"
							insert_after({ mkTok(TT::TK_PARENTHESIS_CLOSE), mkTok(TT::TK_PARENTHESIS_CLOSE) }, assign_closure_end);
							reset_to(cur_tok);

						} break;

						default:
							break;
					}

					// Check for right-hand usage: if previous token is anything but a `{`, `}` or `;`.
					if (peek_prev_tk_type() == TT::TK_SEMICOLON ||
							peek_prev_tk_type() == TT::TK_CURLY_BRACKET_OPEN ||
							peek_prev_tk_type() == TT::TK_CURLY_BRACKET_CLOSE) {
						break;
					}

					// Invert right-hand usage.
					Vector<Token> right_hand_prefix = { // "(1.0 - (";
						mkTok(TT::TK_PARENTHESIS_OPEN),
						mkTok(TT::TK_FLOAT_CONSTANT, {}, 1.0),
						mkTok(TT::TK_SPACE),
						mkTok(TT::TK_OP_SUB),
						mkTok(TT::TK_SPACE)
					};
					if (assign_closure_end) {
						right_hand_prefix.append_array({ mkTok(TT::TK_PARENTHESIS_OPEN) });
						insert_after({ mkTok(TT::TK_PARENTHESIS_CLOSE), mkTok(TT::TK_PARENTHESIS_CLOSE) }, assign_closure_end);
					} else {
						insert_after(mkTok(TT::TK_PARENTHESIS_CLOSE), cur_tok);
					}
					insert_before(right_hand_prefix, cur_tok);
				} else if (has_builtin_rename(shader_mode, cur_tok_text, curr_func) && !scope_has_decl(curr_func, cur_tok_text)) {
					String rename = get_builtin_rename(cur_tok_text);
					cur_tok = replace_curr(mkTok(TT::TK_IDENTIFIER, rename), "Built-in '%s' is renamed to '%s'.");
				}
			} break; // End of identifier case.
			case TT::TK_ERROR: {
				LINE_MSG_FAIL(cur_tok->get().line, "Parser error ( " + cur_tok->get().text + ")");
			} break;
			default:
				break;
		}
	}
	return true;
}

String ShaderDeprecatedConverter::emit_code() const {
	if (code_tokens.size() == 0) {
		return "";
	}
	String new_code = "";
	const TokenE *start = code_tokens.front()->next(); // skip TK_EOF token at start
	for (const TokenE *E = start; E; E = E->next()) {
		const Token &tk = E->get();
		ERR_FAIL_COND_V(tk.type < 0 || tk.type > TT::TK_MAX, "");
		bool end = false;
		String tok_text = get_token_literal_text(tk);
		switch (tk.type) {
			case TT::TK_ERROR:
			case TT::TK_EOF: {
				end = true;
			} break;
			case TT::TK_BLOCK_COMMENT: {
				if (tk.pos == NEW_IDENT) {
					if (warning_comments && tok_text.contains("!convert WARNING:")) {
						new_code += tok_text;
					} else if (verbose_comments && tok_text.contains("!convert")) {
						new_code += tok_text;
					} else {
						break;
					}
				} else {
					new_code += tok_text;
				}
			} break;
			default: {
				new_code += tok_text;
			} break;
		}
		if (end) {
			break;
		}
	}

	return new_code;
}

void ShaderDeprecatedConverter::set_warning_comments(bool p_add_comments) {
	warning_comments = p_add_comments;
}
void ShaderDeprecatedConverter::set_fail_on_unported(bool p_fail_on_unported) {
	fail_on_unported = p_fail_on_unported;
}
void ShaderDeprecatedConverter::set_verbose_comments(bool p_verbose_comments) {
	verbose_comments = p_verbose_comments;
}

#endif // DISABLE_DEPRECATED
