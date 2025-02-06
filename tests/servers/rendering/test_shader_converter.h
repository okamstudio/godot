/**************************************************************************/
/*  test_shader_converter.h                                               */
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

#ifndef TEST_SHADER_CONVERTER_H
#define TEST_SHADER_CONVERTER_H

#ifndef DISABLE_DEPRECATED
#include "servers/rendering/shader_converter.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering/shader_types.h"
#include "tests/test_macros.h"

#include <cctype>

namespace TestShaderConverter {

void erase_all_empty(Vector<String> &p_vec) {
	int idx = p_vec.find(" ");
	while (idx >= 0) {
		p_vec.remove_at(idx);
		idx = p_vec.find(" ");
	}
}

bool is_variable_char(unsigned char c) {
	return std::isalnum(c) || c == '_';
}

bool is_operator_char(unsigned char c) {
	return (c == '*') || (c == '+') || (c == '-') || (c == '/') || ((c >= '<') && (c <= '>'));
}

// Remove unnecessary spaces from a line.
String remove_spaces(String &p_str) {
	String res;
	// Result is guaranteed to not be longer than the input.
	res.resize(p_str.size());
	int wp = 0;
	char32_t last = 0;
	bool has_removed = false;

	for (int n = 0; n < p_str.size(); n++) {
		// These test cases only use ASCII.
		unsigned char c = static_cast<unsigned char>(p_str[n]);
		if (std::isblank(c)) {
			has_removed = true;
		} else {
			if (has_removed) {
				// Insert a space to avoid joining things that could potentially form a new token.
				// E.g. "float x" or "- -".
				if ((is_variable_char(c) && is_variable_char(last)) ||
						(is_operator_char(c) && is_operator_char(last))) {
					res[wp++] = ' ';
				}
				has_removed = false;
			}
			res[wp++] = c;
			last = c;
		}
	}
	res.resize(wp);
	return res;
}

// The pre-processor changes indentation and inserts spaces when inserting macros.
// Re-format the code, without changing its meaning, to make it easier to compare.
String compact_spaces(String &p_str) {
	Vector<String> lines = p_str.split("\n", false);
	erase_all_empty(lines);
	for (String &line : lines) {
		line = remove_spaces(line);
	}
	return String("\n").join(lines);
}

void get_keyword_set(HashSet<String> &p_keywords) {
	List<String> keywords;
	ShaderLanguage::get_keyword_list(&keywords);
	for (const String &keyword : keywords) {
		p_keywords.insert(keyword);
	}
}

#define CHECK_SHADER_EQ(a, b) CHECK_EQ(compact_spaces(a), compact_spaces(b))
#define CHECK_SHADER_NE(a, b) CHECK_NE(compact_spaces(a), compact_spaces(b))

void get_compile_info(ShaderLanguage::ShaderCompileInfo &info, RenderingServer::ShaderMode p_mode) {
	info.functions = ShaderTypes::get_singleton()->get_functions(p_mode);
	info.render_modes = ShaderTypes::get_singleton()->get_modes(p_mode);
	info.shader_types = ShaderTypes::get_singleton()->get_types();
	// Only used by editor for completion, so it's not important for these tests.
	info.global_shader_uniform_type_func = [](const StringName &p_name) -> ShaderLanguage::DataType {
		return ShaderLanguage::TYPE_SAMPLER2D;
	};
}

RenderingServer::ShaderMode get_shader_mode(const String &p_mode_string) {
	if (p_mode_string == "canvas_item") {
		return RS::SHADER_CANVAS_ITEM;
	} else if (p_mode_string == "particles") {
		return RS::SHADER_PARTICLES;
	} else if (p_mode_string == "spatial") {
		return RS::SHADER_SPATIAL;
	} else if (p_mode_string == "sky") {
		return RS::SHADER_SKY;
	} else if (p_mode_string == "fog") {
		return RS::SHADER_FOG;
	} else {
		return RS::SHADER_MAX;
	}
}

String get_shader_mode_name(const RenderingServer::ShaderMode &p_mode_string) {
	switch (p_mode_string) {
		case RS::SHADER_CANVAS_ITEM:
			return "canvas_item";
		case RS::SHADER_PARTICLES:
			return "particles";
		case RS::SHADER_SPATIAL:
			return "spatial";
		case RS::SHADER_SKY:
			return "sky";
		case RS::SHADER_FOG:
			return "fog";
		default:
			return "unknown";
	}
}
using SL = ShaderLanguage;
using SDC = ShaderDeprecatedConverter;

#define TEST_CONVERSION(m_old_code, m_expected, m_is_deprecated)                \
	{                                                                           \
		ShaderDeprecatedConverter _i_converter;                                 \
		CHECK_EQ(_i_converter.is_code_deprecated(m_old_code), m_is_deprecated); \
		CHECK(_i_converter.convert_code(m_old_code));                           \
		String _i_new_code = _i_converter.emit_code();                          \
		CHECK_SHADER_EQ(_i_new_code, m_expected);                               \
	}

#define TEST_CONVERSION_COMPILE(m_old_code, m_expected, m_is_deprecated)        \
	{                                                                           \
		ShaderLanguage _i_language;                                             \
		String _i_type = SL::get_shader_type(m_old_code);                       \
		ShaderLanguage::ShaderCompileInfo _i_info;                              \
		get_compile_info(_i_info, get_shader_mode(_i_type));                    \
		ShaderDeprecatedConverter _i_converter;                                 \
		CHECK_EQ(_i_converter.is_code_deprecated(m_old_code), m_is_deprecated); \
		CHECK_EQ(_i_converter.get_error_text(), "");                            \
		CHECK(_i_converter.convert_code(m_old_code));                           \
		String _i_new_code = _i_converter.emit_code();                          \
		CHECK_SHADER_EQ(_i_new_code, m_expected);                               \
		Error ret = _i_language.compile(_i_new_code, _i_info);                  \
		CHECK_EQ(_i_language.get_error_text(), "");                             \
		CHECK_EQ(ret, Error::OK);                                               \
	}

TEST_CASE("[ShaderDeprecatedConverter] Simple conversion with arrays") {
	String code = "shader_type particles; void vertex() { float xy[2] = {1.0,1.1}; }";
	String expected = "shader_type particles; void process() { float xy[2] = {1.0,1.1}; }";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] Test warning comments") {
	// Test that warning comments are added when fail_on_unported is false and warning_comments is true
	String code = "shader_type spatial;\nrender_mode specular_phong;";
	String expected = "shader_type spatial;\n/* !convert WARNING: Deprecated render mode 'specular_phong' is not supported by this version of Godot. */\nrender_mode specular_phong;";
	ShaderDeprecatedConverter converter;
	CHECK(converter.is_code_deprecated(code));
	converter.set_warning_comments(true);
	converter.set_fail_on_unported(false);
	CHECK(converter.convert_code(code));
	String new_code = converter.emit_code();
	CHECK_EQ(new_code, expected);
}

TEST_CASE("[ShaderDeprecatedConverter] Simple conversion with arrays and structs") {
	String code = "shader_type particles; struct foo{float bar;} void vertex() { float xy[2] = {1.0,1.1}; }";
	String expected = "shader_type particles; struct foo{float bar;} void process() { float xy[2] = {1.0,1.1}; }";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] new-style array declaration") {
	String code = "shader_type spatial; void vertex() { float[2] xy = {1.0,1.1}; }";
	// code should be the same
	TEST_CONVERSION(code, code, false);
}

TEST_CASE("[ShaderDeprecatedConverter] Simple conversion") {
	String code = "shader_type particles; void vertex() { float x = 1.0; }";
	String expected = "shader_type particles; void process() { float x = 1.0; }";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] Replace non-conformant float literals") {
	String code = "shader_type spatial; const float x = 1f;";
	String expected = "shader_type spatial; const float x = 1.0f;";
	TEST_CONVERSION(code, expected, true);
}

TEST_CASE("[ShaderDeprecatedConverter] particles::vertex() -> particles::process()") {
	SUBCASE("basic") {
		String code = "shader_type particles; void vertex() { float x = 1.0; }";
		String expected = "shader_type particles; void process() { float x = 1.0; }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("with another function named `process` without correct signature") {
		String code = "shader_type particles; void vertex() {}  float process() { return 1.0; }";
		String expected = "shader_type particles; void process() {}  float process_() { return 1.0; }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("with another function named `process` with correct signature") {
		String code = "shader_type particles; void vertex() {}  void process() {}";
		// Should be unchanged.
		TEST_CONVERSION(code, code, false);
	}

	SUBCASE("with another function named `process` that is called") {
		String code = "shader_type particles; float process() { return 1.0; } void vertex() { float foo = process(); }";
		String expected = "shader_type particles; float process_() { return 1.0; } void process() { float foo = process_(); }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("with another function named `process` which calls `vertex`") {
		String code = "shader_type particles; float process() {foo(); return 1.0;} void vertex() {} void foo() { vertex(); }";
		String expected = "shader_type particles; float process_() {foo(); return 1.0;} void process() {} void foo() { process(); }";
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("No function named `vertex`") {
		String code = "shader_type particles; void process() {}";
		// Should be unchanged.
		TEST_CONVERSION(code, code, false);
	}
}

TEST_CASE("[ShaderDeprecatedConverter] CLEARCOAT_GLOSS -> CLEARCOAT_ROUGHNESS") {
	SUBCASE("Left-hand simple assignment") {
		String code("shader_type spatial; void fragment() {\n"
					"CLEARCOAT_GLOSS = 1.0;\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"CLEARCOAT_ROUGHNESS = (1.0 - (1.0));\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("Left-hand *= assignment") {
		String code("shader_type spatial; void fragment() {\n"
					"CLEARCOAT_GLOSS *= 0.5;\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"CLEARCOAT_ROUGHNESS = (1.0 - ((1.0 - CLEARCOAT_ROUGHNESS) * 0.5));\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("Right-hand usage") {
		String code("shader_type spatial; void fragment() {\n"
					"float foo = CLEARCOAT_GLOSS;\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"float foo = (1.0 - CLEARCOAT_ROUGHNESS);\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("both usages") {
		String code("shader_type spatial; void fragment() {\n"
					"float foo = (CLEARCOAT_GLOSS *= 0.5);\n"
					"}\n");
		String expected("shader_type spatial; void fragment() {\n"
						"float foo = ((1.0 - (CLEARCOAT_ROUGHNESS = (1.0 - ((1.0 - CLEARCOAT_ROUGHNESS) * 0.5)))));\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
}
TEST_CASE("[ShaderDeprecatedConverter] Wrap INDEX in int()") {
	SUBCASE("basic") {
		String code("shader_type particles; void vertex() {\n"
					"int foo = INDEX/2;\n"
					"}\n");
		String expected("shader_type particles; void process() {\n"
						"int foo = int(INDEX)/2;\n"
						"}\n");

		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("Wrapped in pre-existing cast") {
		String code("shader_type particles; void vertex() {\n"
					"float foo = float(INDEX/2);\n"
					"}\n");
		String expected("shader_type particles; void process() {\n"
						"float foo = float(int(INDEX)/2);\n"
						"}\n");

		TEST_CONVERSION(code, expected, true);
	}
	SUBCASE("Without clobbering existing casts") {
		String code("shader_type particles; void vertex() {\n"
					"int foo = int(INDEX/2) * int(INDEX) * 2;\n"
					"float bar = float(INDEX);\n"
					"}\n");
		String expected("shader_type particles; void process() {\n"
						"int foo = int(int(INDEX)/2) * int(INDEX) * 2;\n"
						"float bar = float(INDEX);\n"
						"}\n");
		TEST_CONVERSION(code, expected, true);
	}
}

TEST_CASE("[ShaderDeprecatedConverter] All hint renames") {
	String code_template = "shader_type spatial; uniform sampler2D foo : %s;";
	// get all the hint renames
	List<String> hints;
	ShaderDeprecatedConverter::_get_hint_renames_list(&hints);

	SUBCASE("No renamed hints present in current keyword list") {
		HashSet<String> keywords_set;
		get_keyword_set(keywords_set);
		for (const String &hint : hints) {
			CHECK_FALSE(keywords_set.has(hint));
		}
	}

	SUBCASE("All renamed hints are replaced") {
		for (const String &hint : hints) {
			ShaderDeprecatedConverter::TokenType type = ShaderDeprecatedConverter::get_hint_replacement(hint);
			String rename = ShaderDeprecatedConverter::get_tokentype_text(type);
			String code = vformat(code_template, hint);
			String expected = vformat(code_template, rename);
			TEST_CONVERSION(code, expected, true);
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Built-in renames") {
	// Get all the built-in renames.
	List<String> builtins;
	ShaderDeprecatedConverter::_get_builtin_renames_list(&builtins);
	// remove built-ins that have special handling, we test those above
	for (List<String>::Element *E = builtins.front(); E; E = E->next()) {
		if (ShaderDeprecatedConverter::_rename_has_special_handling(E->get())) {
			List<String>::Element *prev = E->prev();
			builtins.erase(E);
			E = prev;
		}
	}
	Vector<RS::ShaderMode> modes = { RS::SHADER_SPATIAL, RS::SHADER_CANVAS_ITEM, RS::SHADER_PARTICLES };
	HashMap<RS::ShaderMode, HashMap<String, Vector<String>>> rename_func_map;
	for (RS::ShaderMode mode : modes) {
		rename_func_map[mode] = HashMap<String, Vector<String>>();
		for (const String &builtin : builtins) {
			rename_func_map[mode][builtin] = ShaderDeprecatedConverter::_get_funcs_builtin_rename(mode, builtin);
		}
	}

	SUBCASE("All renamed built-ins are not currently built-in") {
		for (RS::ShaderMode mode : modes) {
			ShaderLanguage::ShaderCompileInfo info;
			get_compile_info(info, mode);
			for (const String &builtin : builtins) {
				// Now get the funcs applicable for this mode and built-in.
				for (const String &func : rename_func_map[mode][builtin]) {
					// The built-in should not be present in the built-ins list.
					auto &finfo = info.functions[func];
					if (finfo.built_ins.has(builtin)) {
						WARN_PRINT(vformat("Renamed 3.x Built-in %s is present in function %s", builtin, func));
					}
					CHECK_FALSE(finfo.built_ins.has(builtin));
				}
			}
		}
	}

	SUBCASE("All renamed built-ins are replaced") {
		String code_template = "shader_type %s; void %s() { %s; }";
		for (RS::ShaderMode mode : modes) {
			for (const String &builtin : builtins) {
				// Now get the funcs applicable for this mode and built-in.
				String rename = ShaderDeprecatedConverter::get_builtin_rename(builtin);
				for (const String &func : rename_func_map[mode][builtin]) {
					String code = vformat(code_template, get_shader_mode_name(mode), func, builtin);
					String expected = vformat(code_template, get_shader_mode_name(mode), func, rename);
					TEST_CONVERSION(code, expected, true);
				}
			}
		}
	}
	SUBCASE("No renaming built-ins in non-candidate functions") {
		String code_template = "shader_type %s; void %s() { float %s = 1.0; %s += 1.0; }";
		for (RS::ShaderMode mode : modes) {
			ShaderLanguage::ShaderCompileInfo info;
			get_compile_info(info, mode);
			for (const String &builtin : builtins) {
				Vector<String> non_funcs;
				for (KeyValue<StringName, ShaderLanguage::FunctionInfo> &func : info.functions) {
					if (func.key == "global") {
						continue;
					}
					if (!rename_func_map[mode][builtin].has(func.key)) {
						non_funcs.push_back(func.key);
					}
				}
				String rename = ShaderDeprecatedConverter::get_builtin_rename(builtin);
				for (const String &func : non_funcs) {
					String code = vformat(code_template, get_shader_mode_name(mode), func, builtin, builtin);
					// The code should not change.
					TEST_CONVERSION(code, code, false);
				}
			}
		}
	}
	SUBCASE("No renaming built-ins in candidate functions with built-in declared") {
		String code_template = "shader_type %s; void %s() { float %s = 1.0; %s += 1.0; }";
		for (RS::ShaderMode mode : modes) {
			for (const String &builtin : builtins) {
				for (const String &func : rename_func_map[mode][builtin]) {
					String code = vformat(code_template, get_shader_mode_name(mode), func, builtin, builtin);
					// The code should not change.
					TEST_CONVERSION(code, code, false);
				}
			}
		}
	}
}

// TODO: Remove this when the MODULATE built-in PR lands.
// If this fails, remove the MODULATE entry from ShaderDeprecatedConverter::removed_builtins, then remove this test and the following test.
TEST_CASE("[ShaderDeprecatedConverter] MODULATE is not a built-in") {
	ShaderLanguage::ShaderCompileInfo info;
	get_compile_info(info, RS::ShaderMode::SHADER_CANVAS_ITEM);
	SUBCASE("MODULATE is not a built-in") {
		for (const String &func : Vector<String>{ "vertex", "fragment", "light" }) {
			auto &finfo = info.functions[func];
			CHECK_FALSE(finfo.built_ins.has("MODULATE"));
		}
	}
}

// Don't remove this one if the above doesn't fail too.
TEST_CASE("[ShaderDeprecatedConverter] MODULATE handling") {
	ShaderLanguage::ShaderCompileInfo info;
	get_compile_info(info, RS::ShaderMode::SHADER_CANVAS_ITEM);
	SUBCASE("Fails to compile") {
		for (const String &func : Vector<String>{ "vertex", "fragment", "light" }) {
			String code = vformat("shader_type canvas_item; void %s() { MODULATE; }", func);
			ShaderLanguage sl;
			CHECK_NE(sl.compile(code, info), Error::OK);
		}
	}
	SUBCASE("Fails to convert on fail_on_unported=true") {
		for (const String &func : Vector<String>{ "vertex", "fragment", "light" }) {
			String code = vformat("shader_type canvas_item; void %s() { MODULATE; }", func);
			ShaderDeprecatedConverter converter;
			CHECK(converter.is_code_deprecated(code));
			converter.set_fail_on_unported(true);
			CHECK_FALSE(converter.convert_code(code));
		}
	}

	SUBCASE("Conversion succeeds on fail_on_unported=false") {
		for (const String &func : Vector<String>{ "vertex", "fragment", "light" }) {
			String code = vformat("shader_type canvas_item; void %s() { MODULATE; }", func);
			ShaderDeprecatedConverter converter;
			CHECK(converter.is_code_deprecated(code));
			converter.set_fail_on_unported(false);
			CHECK(converter.convert_code(code));
			String new_code = converter.emit_code();
			CHECK(new_code.find("/*") != -1);
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Uniform declarations for removed builtins") {
	// Test uniform declaration inserts for removed builtins for all shader types.
	String code_template = "shader_type %s;%s void %s() { %s; }";
	String uniform_template = "\nuniform %s %s : %s;\n";
	// Get all the removed built-ins.
	List<String> builtins;
	ShaderDeprecatedConverter::_get_builtin_removals_list(&builtins);
	Vector<RS::ShaderMode> modes = { RS::SHADER_SPATIAL, RS::SHADER_CANVAS_ITEM, RS::SHADER_PARTICLES };
	HashMap<RS::ShaderMode, ShaderLanguage::ShaderCompileInfo> compiler_infos;

	SUBCASE("Removed built-ins are not currently built-in") {
		for (RS::ShaderMode mode : modes) {
			ShaderLanguage::ShaderCompileInfo info;
			get_compile_info(info, mode);
			for (const String &builtin : builtins) {
				Vector<String> funcs = ShaderDeprecatedConverter::_get_funcs_builtin_removal(mode, builtin);
				for (const String &func : funcs) {
					const ShaderLanguage::FunctionInfo &finfo = info.functions[func];
					if (finfo.built_ins.has(builtin)) {
						WARN_PRINT(vformat("Removed 3.x Built-in %s is present in function %s", builtin, func));
					}
					CHECK_FALSE(finfo.built_ins.has(builtin));
				}
			}
		}
	}

	SUBCASE("All removed built-ins have uniform declarations") {
		for (RS::ShaderMode mode : modes) {
			for (const String &builtin : builtins) {
				// now get the funcs applicable for this mode and builtins
				ShaderLanguage::TokenType type = ShaderDeprecatedConverter::get_removed_builtin_uniform_type(builtin);
				if (type == ShaderDeprecatedConverter::TokenType::TK_ERROR) {
					continue;
				}
				Vector<ShaderLanguage::TokenType> hints = ShaderDeprecatedConverter::get_removed_builtin_hints(builtin);
				Vector<String> funcs = ShaderDeprecatedConverter::_get_funcs_builtin_removal(mode, builtin);
				String hint_string = "";
				for (int i = 0; i < hints.size(); i++) {
					hint_string += ShaderDeprecatedConverter::get_tokentype_text(hints[i]);
					if (i < hints.size() - 1) {
						hint_string += ", ";
					}
				}
				String uniform_decl = vformat(uniform_template, ShaderDeprecatedConverter::get_tokentype_text(type), builtin, hint_string);
				for (const String &func : funcs) {
					String code = vformat(code_template, get_shader_mode_name(mode), "", func, builtin);
					if (type == ShaderDeprecatedConverter::TokenType::TK_ERROR) { // Unported builtins with no uniform declaration
						ShaderDeprecatedConverter converter;
						CHECK(converter.is_code_deprecated(code));
						CHECK_FALSE(converter.convert_code(code));
						converter.set_fail_on_unported(false);
						CHECK(converter.convert_code(code));
						continue;
					}
					String expected = vformat(code_template, get_shader_mode_name(mode), uniform_decl, func, builtin);
					TEST_CONVERSION(code, expected, true);
				}
			}
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Handling of renamed render_modes") {
	List<String> render_modes;
	ShaderDeprecatedConverter::_get_render_mode_renames_list(&render_modes);
	HashSet<String> render_modes_set;
	for (const String &render_mode : render_modes) {
		render_modes_set.insert(render_mode);
	}
	// static const char *code_template = "shader_type %s; render_mode %s;";
	static const char *code_template = "shader_type %s; render_mode blend_mix, %s, depth_draw_always;";
	SUBCASE("Renamed render modes are not currently valid render modes") {
		ShaderLanguage::ShaderCompileInfo info;
		get_compile_info(info, RS::SHADER_SPATIAL);
		for (const auto &render_mode : info.render_modes) {
			if (render_modes_set.has(render_mode.name)) {
				WARN_PRINT(vformat("Renamed 3.x Render Mode %s is present in render modes", render_mode.name));
			}
			CHECK_FALSE(render_modes_set.has(render_mode.name));
		}
	}
	for (const String &render_mode : render_modes) {
		SUBCASE((render_mode + "is renamed").utf8().get_data()) {
			String rename = ShaderDeprecatedConverter::get_render_mode_rename(render_mode);
			String code = vformat(code_template, "spatial", render_mode);
			String expected = vformat(code_template, "spatial", rename);
			TEST_CONVERSION(code, expected, true);
		}
		SUBCASE((render_mode + "is renamed").utf8().get_data()) {
			String rename = ShaderDeprecatedConverter::get_render_mode_rename(render_mode);
			String code = vformat(code_template, "spatial", render_mode);
			String expected = vformat(code_template, "spatial", rename);
			TEST_CONVERSION(code, expected, true);
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Handling of removed render_modes") {
	List<String> removed_render_modes;
	ShaderDeprecatedConverter::_get_render_mode_removals_list(&removed_render_modes);
	HashSet<String> render_modes_set;
	for (const String &render_mode : removed_render_modes) {
		render_modes_set.insert(render_mode);
	}
	SUBCASE("Removed render modes are not currently valid render modes") {
		ShaderLanguage::ShaderCompileInfo info;
		get_compile_info(info, RS::SHADER_SPATIAL);
		for (const auto &render_mode : info.render_modes) {
			if (render_modes_set.has(render_mode.name)) {
				WARN_PRINT(vformat("Removed 3.x Render Mode %s is present in render modes", render_mode.name));
			}
			CHECK_FALSE(render_modes_set.has(render_mode.name));
		}
	}
	const char *code_template = "shader_type %s; %s";
	static const char *render_mode_template = "render_mode blend_mix, %s, depth_draw_always;";
	static const char *render_mode_removed = "render_mode blend_mix, depth_draw_always;";

	for (const String &render_mode : removed_render_modes) {
		SUBCASE((render_mode + " is handled").utf8().get_data()) {
			String render_mode_decl = vformat(render_mode_template, render_mode);
			String code = vformat(code_template, "spatial", render_mode_decl);
			String expected = vformat(code_template, "spatial", ShaderDeprecatedConverter::can_remove_render_mode(render_mode) ? render_mode_removed : render_mode_decl);
			ShaderDeprecatedConverter converter;
			CHECK(converter.is_code_deprecated(code));
			converter.set_warning_comments(false);
			converter.set_fail_on_unported(false);
			CHECK(converter.convert_code(code));
			String new_code = converter.emit_code();
			CHECK_SHADER_EQ(new_code, expected);
			// Check for warning comment
			converter.set_warning_comments(true);
			new_code = converter.emit_code();
			CHECK(new_code.contains("/* !convert WARNING:"));
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Renaming of functions with the same name as new built-in functions") {
	List<String> builtins;
	ShaderDeprecatedConverter::_get_new_builtin_funcs_list(&builtins);

	for (const String &builtin : builtins) {
		SUBCASE((builtin + " renamed").utf8().get_data()) {
			String code = vformat("shader_type spatial; void %s() { %s(); }", builtin, builtin);
			String expected = vformat("shader_type spatial; void %s_() { %s_(); }", builtin, builtin);
			TEST_CONVERSION(code, expected, true);
		}

		SUBCASE((builtin + " usage without declaration not replaced").utf8().get_data()) {
			String code = vformat("shader_type spatial; void foo() { %s(); }", builtin);
			TEST_CONVERSION(code, code, false);
		}
		SUBCASE((builtin + " renamed while handling usage as variable").utf8().get_data()) {
			const char *test_format = "shader_type spatial;\nvoid foo(){%s();}\nvoid %s() { float %s = 3.0; }\n";
			String code = vformat(test_format, builtin, builtin, builtin);
			String last_builtin = builtin;
			if (ShaderDeprecatedConverter::tokentype_is_new_reserved_keyword(ShaderDeprecatedConverter::get_tokentype_from_text(builtin))) {
				last_builtin += "_";
			}
			String expected = vformat(test_format, builtin + "_", builtin + "_", last_builtin);
			TEST_CONVERSION(code, expected, true);
		}
	}
}

// Reserved keywords (i.e. non-built-in function keywords that have a discrete token type)
TEST_CASE("[ShaderDeprecatedConverter] Replacement of reserved keywords used as identifiers") {
	Vector<String> keywords;
	for (int i = 0; i < ShaderLanguage::TK_MAX; i++) {
		if (ShaderDeprecatedConverter::tokentype_is_new_reserved_keyword(static_cast<ShaderLanguage::TokenType>(i))) {
			keywords.push_back(ShaderDeprecatedConverter::get_tokentype_text(static_cast<ShaderLanguage::TokenType>(i)));
		}
	}
	Vector<String> hint_keywords;
	for (int i = 0; i < SL::TK_MAX; i++) {
		if (SDC::tokentype_is_new_hint(static_cast<SL::TokenType>(i))) {
			hint_keywords.push_back(SDC::get_tokentype_text(static_cast<SL::TokenType>(i)));
		}
	}
	Vector<String> uniform_quals;
	for (int i = 0; i < SL::TK_MAX; i++) {
		if (SL::is_token_uniform_qual(static_cast<SL::TokenType>(i))) {
			uniform_quals.push_back(SDC::get_tokentype_text(static_cast<SL::TokenType>(i)));
		}
	}
	Vector<String> shader_types_to_test = { "spatial", "canvas_item", "particles" };

	static const char *decl_test_template[]{
		"shader_type %s;\nvoid %s() {}\nvoid foo(){%s();}\n",
		"shader_type %s;\nvoid test_func() {float %s; %s = 1.0;}\n",
		"shader_type %s;\nuniform float %s;\nvoid foo(){float bar = %s * 3.0;}\n",
		"shader_type %s;\nconst float %s = 1.0;\nvoid foo(){float bar = %s * 3.0;}\n",
		"shader_type %s;\nvarying float %s;\nvoid foo(){float bar = %s * 3.0;}\n",
		"shader_type %s;\nstruct foo{float %s;};\nvoid bar(){foo f; f.%s = 1.0;}\n",
		"shader_type %s;\nstruct %s{float foo;};\nvoid bar(){%s f; f.foo = 1.0;}\n",
		nullptr
	};
	// NOTE: if this fails, the current behavior of the converter to replace these has to be changed.
	SUBCASE("Code with reserved keywords used as identifiers fail to compile") {
		ShaderLanguage::ShaderCompileInfo info;
		get_compile_info(info, RS::SHADER_SPATIAL);
		for (const String &shader_type : shader_types_to_test) {
			for (const String &keyword : keywords) {
				for (int i = 0; decl_test_template[i] != nullptr; i++) {
					String code = vformat(decl_test_template[i], shader_type, keyword, keyword);
					ShaderLanguage sl;
					CHECK_NE(sl.compile(code, info), Error::OK);
				}
			}
		}
	}
	SUBCASE("Code with reserved keywords used as identifiers is converted successfully") {
		for (const String &shader_type : shader_types_to_test) {
			ShaderLanguage::ShaderCompileInfo info;
			get_compile_info(info, get_shader_mode(shader_type));
			for (const String &keyword : keywords) {
				for (int i = 0; decl_test_template[i] != nullptr; i++) {
					if (shader_type == "particles" && String(decl_test_template[i]).contains("varying")) {
						continue;
					}
					String code = vformat(decl_test_template[i], shader_type, keyword, keyword);
					String expected = vformat(decl_test_template[i], shader_type, keyword + "_", keyword + "_");
					TEST_CONVERSION_COMPILE(code, expected, true);
				}
			}
		}
	}
	static const char *new_hint_test = "shader_type spatial;\nuniform sampler2D foo : %s; const float %s = 1.0;\n";
	SUBCASE("New hints used as hints are not replaced") {
		for (const String &hint : hint_keywords) {
			String code = vformat(new_hint_test, hint, "bar");
			// Code should not change.
			TEST_CONVERSION(code, code, false);
		}
	}

	SUBCASE("Mixed new hints used as hints and new hints used as identifiers") {
		for (const String &hint : hint_keywords) {
			String code = vformat(new_hint_test, hint, hint);
			// Should not change.
			ShaderDeprecatedConverter converter;
			CHECK_FALSE(converter.is_code_deprecated(code)); // Should be detected as not deprecated.
			converter.set_warning_comments(false);
			CHECK(converter.convert_code(code));
			String new_code = converter.emit_code();
			// Code should not change
			CHECK_EQ(new_code, code);
			// Check for warning comment
			converter.set_warning_comments(true);
			new_code = converter.emit_code();
			CHECK(new_code.contains("/* !convert WARNING:"));
		}
	}
	static const char *non_id_keyword_test = "shader_type spatial;\n%s uniform sampler2D foo; const float %s = 1.0;\n";
	SUBCASE("New keywords not used as identifiers are not replaced") {
		for (const String &qual : uniform_quals) {
			// e.g. "shader_type spatial;\nglobal uniform sampler2D foo; const float bar = 1.0;\n"
			String code = vformat(non_id_keyword_test, qual, "bar");
			// Code should not change.
			TEST_CONVERSION(code, code, false);
		}
	}

	SUBCASE("Mixed idiomatic new reserved words and new reserved words used as identifiers") {
		for (const String &qual : uniform_quals) {
			// e.g. "shader_type spatial;\nglobal uniform sampler2D foo; const float global = 1.0;\n"
			String code = vformat(non_id_keyword_test, qual, qual);
			// Should not change.
			ShaderDeprecatedConverter converter;
			CHECK_FALSE(converter.is_code_deprecated(code)); // Should be detected as not deprecated.
			converter.set_warning_comments(false);
			CHECK(converter.convert_code(code));
			String new_code = converter.emit_code();
			// Code should not change
			CHECK_EQ(new_code, code);
			// Check for warning comment
			converter.set_warning_comments(true);
			new_code = converter.emit_code();
			CHECK(new_code.contains("/* !convert WARNING:"));
		}
	}
}

TEST_CASE("[ShaderDeprecatedConverter] Convert default 3.x nodetree shader") {
	static const char *default_3x_nodtree_shader =
			R"(shader_type spatial;
render_mode blend_mix, depth_draw_always, cull_back, diffuse_burley, specular_schlick_ggx;

uniform sampler2D texture_0: hint_albedo;


void node_bsdf_principled(vec4 color, float subsurface, vec4 subsurface_color,
        float metallic, float specular, float roughness, float clearcoat,
        float clearcoat_roughness, float anisotropy, float transmission,
        float IOR, out vec3 albedo, out float sss_strength_out,
        out float metallic_out, out float specular_out,
        out float roughness_out, out float clearcoat_out,
        out float clearcoat_gloss_out, out float anisotropy_out,
        out float transmission_out, out float ior) {
    metallic = clamp(metallic, 0.0, 1.0);
    transmission = clamp(transmission, 0.0, 1.0);

    subsurface = subsurface * (1.0 - metallic);

    albedo = mix(color.rgb, subsurface_color.rgb, subsurface);
    sss_strength_out = subsurface;
    metallic_out = metallic;
    specular_out = pow((IOR - 1.0)/(IOR + 1.0), 2)/0.08;
    roughness_out = roughness;
    clearcoat_out = clearcoat * (1.0 - transmission);
    clearcoat_gloss_out = 1.0 - clearcoat_roughness;
    anisotropy_out = clamp(anisotropy, 0.0, 1.0);
    transmission_out = (1.0 - transmission) * (1.0 - metallic);
    ior = IOR;
}


void node_tex_image(vec3 co, sampler2D ima, out vec4 color, out float alpha) {
    color = texture(ima, co.xy);
    alpha = color.a;
}

void vertex () {
}

void fragment () {

	// node: 'Image Texture'
	// type: 'ShaderNodeTexImage'
	// input sockets handling
	vec3 node0_in0_vector = vec3(0.0, 0.0, 0.0);
	// output sockets definitions
	vec4 node0_out0_color;
	float node0_out1_alpha;

	node0_in0_vector = vec3(UV, 0.0);
	node_tex_image(node0_in0_vector, texture_0, node0_out0_color, node0_out1_alpha);

	// node: 'Principled BSDF'
	// type: 'ShaderNodeBsdfPrincipled'
	// input sockets handling
	vec4 node1_in0_basecolor = node0_out0_color;
	float node1_in1_subsurface = float(0.0);
	vec3 node1_in2_subsurfaceradius = vec3(1.0, 0.20000000298023224,
		0.10000000149011612);
	vec4 node1_in3_subsurfacecolor = vec4(0.800000011920929, 0.800000011920929,
		0.800000011920929, 1.0);
	float node1_in4_metallic = float(0.0);
	float node1_in5_specular = float(0.5);
	float node1_in6_speculartint = float(0.0);
	float node1_in7_roughness = float(1.0);
	float node1_in8_anisotropic = float(0.0);
	float node1_in9_anisotropicrotation = float(0.0);
	float node1_in10_sheen = float(0.0);
	float node1_in11_sheentint = float(0.5);
	float node1_in12_clearcoat = float(0.0);
	float node1_in13_clearcoatroughness = float(0.029999999329447746);
	float node1_in14_ior = float(1.4500000476837158);
	float node1_in15_transmission = float(0.0);
	float node1_in16_transmissionroughness = float(0.0);
	vec4 node1_in17_emission = vec4(0.0, 0.0, 0.0, 1.0);
	float node1_in18_emissionstrength = float(1.0);
	float node1_in19_alpha = float(1.0);
	vec3 node1_in20_normal = NORMAL;
	vec3 node1_in21_clearcoatnormal = vec3(0.0, 0.0, 0.0);
	vec3 node1_in22_tangent = TANGENT;
	// output sockets definitions
	vec3 node1_bsdf_out0_albedo;
	float node1_bsdf_out1_sss_strength;
	float node1_bsdf_out3_specular;
	float node1_bsdf_out2_metallic;
	float node1_bsdf_out4_roughness;
	float node1_bsdf_out5_clearcoat;
	float node1_bsdf_out6_clearcoat_gloss;
	float node1_bsdf_out7_anisotropy;
	float node1_bsdf_out8_transmission;
	float node1_bsdf_out9_ior;

	node_bsdf_principled(node1_in0_basecolor, node1_in1_subsurface,
		node1_in3_subsurfacecolor, node1_in4_metallic, node1_in5_specular,
		node1_in7_roughness, node1_in12_clearcoat, node1_in13_clearcoatroughness,
		node1_in8_anisotropic, node1_in15_transmission, node1_in14_ior,
		node1_bsdf_out0_albedo, node1_bsdf_out1_sss_strength, node1_bsdf_out2_metallic,
		node1_bsdf_out3_specular, node1_bsdf_out4_roughness, node1_bsdf_out5_clearcoat,
		node1_bsdf_out6_clearcoat_gloss, node1_bsdf_out7_anisotropy,
		node1_bsdf_out8_transmission, node1_bsdf_out9_ior);

	ALBEDO = node1_bsdf_out0_albedo;
	SSS_STRENGTH = node1_bsdf_out1_sss_strength;
	SPECULAR = node1_bsdf_out3_specular;
	METALLIC = node1_bsdf_out2_metallic;
	ROUGHNESS = node1_bsdf_out4_roughness;
	CLEARCOAT = node1_bsdf_out5_clearcoat;
	CLEARCOAT_GLOSS = node1_bsdf_out6_clearcoat_gloss;
	NORMAL = node1_in20_normal;
	// uncomment it when you need it
	// TRANSMISSION = vec3(1.0, 1.0, 1.0) * node1_bsdf_out8_transmission;
	// uncomment it when you are modifying TANGENT
	// TANGENT = normalize(cross(cross(node1_in22_tangent, NORMAL), NORMAL));
	// BINORMAL = cross(TANGENT, NORMAL);
	// uncomment it when you have tangent(UV) set
	// ANISOTROPY = node1_bsdf_out7_anisotropy;
}
	)";

	ShaderLanguage sl;
	ShaderLanguage::ShaderCompileInfo info;
	get_compile_info(info, RS::SHADER_SPATIAL);
	SUBCASE("Default 3.x nodetree shader does not compile") {
		CHECK_NE(sl.compile(default_3x_nodtree_shader, info), Error::OK);
	}
	sl.clear();
	SUBCASE("Convert default 3.x nodetree shader") {
		ShaderDeprecatedConverter converter;
		CHECK(converter.convert_code(default_3x_nodtree_shader));
		String new_code = converter.emit_code();
		CHECK(new_code.find("/*") == -1);
		converter.set_verbose_comments(true);
		new_code = converter.emit_code();
		CHECK(new_code.find("/*") != -1);
		CHECK_FALSE(converter.get_error_line());
	}

	SUBCASE("Converted default 3.x nodetree shader compiles") {
		ShaderDeprecatedConverter converter;
		CHECK(converter.convert_code(default_3x_nodtree_shader));
		String new_code = converter.emit_code();
		CHECK_EQ(sl.compile(new_code, info), Error::OK);
	}
	sl.clear();
}

} // namespace TestShaderConverter
#undef TEST_CONVERSION
#undef TEST_CONVERSION_COMPILE
#endif // DISABLE_DEPRECATED

#endif // TEST_SHADER_CONVERTER_H
