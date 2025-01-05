/**************************************************************************/
/*  test_shader_language.h                                                */
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

#ifndef TEST_SHADER_LANGUAGE_H
#define TEST_SHADER_LANGUAGE_H

#include "servers/rendering/shader_language.h"
#include "servers/rendering/shader_types.h"

#include "tests/test_macros.h"

#include <cctype>

namespace TestShaderLanguage {

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

TEST_CASE("[ShaderLanguage] Minimal Script") {
	ShaderLanguage sl;
	ShaderLanguage::ShaderCompileInfo info;
	get_compile_info(info, RS::SHADER_SPATIAL);
	String code = "shader_type spatial;";
	CHECK_EQ(sl.compile(code, info), Error::OK);
}

// No keywords (except for built-in functions) should be valid identifiers.
TEST_CASE("[ShaderLanguage] Ensure no reserved keywords are valid identifiers") {
	List<String> keywords;
	List<String> builtin_functions;
	ShaderLanguage::get_keyword_list(&keywords);
	ShaderLanguage::get_builtin_funcs(&builtin_functions);

	HashSet<String> builtin_set;
	for (const String &keyword : builtin_functions) {
		builtin_set.insert(keyword);
	}

	HashSet<String> non_func_keywords_set;
	for (const String &keyword : keywords) {
		if (!builtin_set.has(keyword)) {
			non_func_keywords_set.insert(keyword);
		}
	}

	static const char *decl_test_template[]{
		"shader_type %s;\nvoid %s() {}\n",
		"shader_type %s;\nvoid vertex() {float %s;}\n",
		"shader_type %s;\nuniform sampler2D %s;\n",
		"shader_type %s;\nconst float %s = 1.0;\n",
		nullptr
	};
	static const char *varying_template = "shader_type %s;\nvarying float %s;\n";
	Vector<String> non_varying_types = { "particles", "sky", "fog" };

	auto shader_types_to_test = ShaderTypes::get_singleton()->get_types();
	for (auto shader_type : shader_types_to_test) {
		ShaderLanguage::ShaderCompileInfo info;
		get_compile_info(info, get_shader_mode(shader_type));
		// test templates with non-keyword identifiers

		for (int i = 0; decl_test_template[i] != nullptr; i++) {
			String code = vformat(decl_test_template[i], shader_type, "foo");
			String result;
			ShaderLanguage sl;
			CHECK_EQ(sl.compile(code, info), Error::OK);
		}
		if (!non_varying_types.has(shader_type)) {
			String code = vformat(varying_template, shader_type, "foo");
			String result;
			ShaderLanguage sl;
			CHECK_EQ(sl.compile(code, info), Error::OK);
		}

		for (const String &keyword : non_func_keywords_set) {
			for (int i = 0; decl_test_template[i] != nullptr; i++) {
				String code = vformat(decl_test_template[i], shader_type, keyword);
				String result;
				ShaderLanguage sl;
				CHECK_NE(sl.compile(code, info), Error::OK);
			}
			if (!non_varying_types.has(shader_type)) {
				String code = vformat(varying_template, shader_type, keyword);
				String result;
				ShaderLanguage sl;
				CHECK_NE(sl.compile(code, info), Error::OK);
			}
		}
	}
}

} // namespace TestShaderLanguage

#endif // TEST_SHADER_LANGUAGE_H
