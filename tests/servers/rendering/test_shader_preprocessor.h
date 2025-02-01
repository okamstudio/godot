/**************************************************************************/
/*  test_shader_preprocessor.h                                            */
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

#pragma once

#include "servers/rendering/shader_preprocessor.h"

#include "tests/test_macros.h"

#include <cctype>

namespace TestShaderPreprocessor {

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

#define CHECK_SHADER_EQ(a, b) CHECK_EQ(compact_spaces(a), compact_spaces(b))
#define CHECK_SHADER_NE(a, b) CHECK_NE(compact_spaces(a), compact_spaces(b))

TEST_CASE("[ShaderPreprocessor] Simple defines") {
	String code(
			"#define X 1.0 // comment\n"
			"#define Y mix\n"
			"#define Z X\n"
			"\n"
			"#define func0 \\\n"
			"  vec3 my_fun(vec3 arg) {\\\n"
			"    return pow(arg, 2.2);\\\n"
			"  }\n"
			"\n"
			"func0\n"
			"\n"
			"fragment() {\n"
			"  ALBEDO = vec3(X);\n"
			"  float x = Y(0., Z, X);\n"
			"  #undef X\n"
			"  float X = x;\n"
			"  x = -Z;\n"
			"}\n");
	String expected(
			"vec3 my_fun(vec3 arg) { return pow(arg, 2.2); }\n"
			"\n"
			"fragment() {\n"
			"  ALBEDO = vec3( 1.0 );\n"
			"  float x = mix(0., 1.0 , 1.0 );\n"
			"  float X = x;\n"
			"  x = -X;\n"
			"}\n");
	String result;

	ShaderPreprocessor preprocessor;
	CHECK_EQ(preprocessor.preprocess(code, String("file.gdshader"), result), Error::OK);

	CHECK_SHADER_EQ(result, expected);
}

TEST_CASE("[ShaderPreprocessor] Avoid merging adjacent tokens") {
	String code(
			"#define X -10\n"
			"#define Y(s) s\n"
			"\n"
			"fragment() {\n"
			"  float v = 1.0-X-Y(-2);\n"
			"}\n");
	String expected(
			"fragment() {\n"
			"  float v = 1.0 - -10 - -2;\n"
			"}\n");
	String result;

	ShaderPreprocessor preprocessor;
	CHECK_EQ(preprocessor.preprocess(code, String("file.gdshader"), result), Error::OK);

	CHECK_SHADER_EQ(result, expected);
}

TEST_CASE("[ShaderPreprocessor] Complex defines") {
	String code(
			"const float X = 2.0;\n"
			"#define A(X) X*2.\n"
			"#define X 1.0\n"
			"#define Y Z(X, W)\n"
			"#define Z max\n"
			"#define C(X, Y) Z(A(Y), B(X))\n"
			"#define W -X\n"
			"#define B(X) X*3.\n"
			"\n"
			"fragment() {\n"
			"  float x = Y;\n"
			"  float y = C(5., 7.0);\n"
			"}\n");
	String expected(
			"const float X = 2.0;\n"
			"fragment() {\n"
			"  float x = max(1.0, - 1.0);\n"
			"  float y = max(7.0*2. , 5.*3.);\n"
			"}\n");
	String result;

	ShaderPreprocessor preprocessor;
	CHECK_EQ(preprocessor.preprocess(code, String("file.gdshader"), result), Error::OK);

	CHECK_SHADER_EQ(result, expected);
}

TEST_CASE("[ShaderPreprocessor] Concatenation") {
	String code(
			"fragment() {\n"
			"  #define X 1 // this is fine ##\n"
			"  #define y 2\n"
			"  #define z 3##.## 1## 4 ## 59\n"
			"  #define Z(y) X ## y\n"
			"  #define Z2(y) y##X\n"
			"  #define W(y) X, y\n"
			"  #define A(x) fl## oat a = 1##x ##.3  ##  x\n"
			"  #define C(x, y) x##.##y\n"
			"  #define J(x) x##=\n"
			"  float Z(y) = 1.2;\n"
			"  float Z(z) = 2.3;\n"
			"  float Z2(y) = z;\n"
			"  float Z2(z) = 2.3;\n"
			"  int b = max(W(3));\n"
			"  Xy J(+) b J(=) 3 ? 0.1 : 0.2;\n"
			"  A(9);\n"
			"  Xy = C(X, y);\n"
			"}\n");
	String expected(
			"fragment() {\n"
			"  float Xy = 1.2;\n"
			"  float Xz = 2.3;\n"
			"  float yX = 3.1459;\n"
			"  float zX = 2.3;\n"
			"  int b = max(1, 3);\n"
			"  Xy += b == 3 ? 0.1 : 0.2;\n"
			"  float a = 19.39;\n"
			"  Xy = 1.2;\n"
			"}\n");
	String result;

	ShaderPreprocessor preprocessor;
	CHECK_EQ(preprocessor.preprocess(code, String("file.gdshader"), result), Error::OK);

	CHECK_SHADER_EQ(result, expected);
}

TEST_CASE("[ShaderPreprocessor] Nested concatenation") {
	// Concatenation ## should not expand adjacent tokens if they are macros,
	// but this is currently not implemented in Godot's shader preprocessor.
	// To force expanding, an extra macro should be required (B in this case).

	String code(
			"fragment() {\n"
			"  vec2 X = vec2(0);\n"
			"  #define X 1\n"
			"  #define y 2\n"
			"  #define B(x, y) C(x, y)\n"
			"  #define C(x, y) x##.##y\n"
			"  C(X, y) = B(X, y);\n"
			"}\n");
	String expected(
			"fragment() {\n"
			"  vec2 X = vec2(0);\n"
			"  X.y = 1.2;\n"
			"}\n");
	String result;

	ShaderPreprocessor preprocessor;
	CHECK_EQ(preprocessor.preprocess(code, String("file.gdshader"), result), Error::OK);

	// TODO: Reverse the check when/if this is changed.
	CHECK_SHADER_NE(result, expected);
}

TEST_CASE("[ShaderPreprocessor] Concatenation sorting network") {
	String code(
			"fragment() {\n"
			"  #define ARR(X) test##X\n"
			"  #define ACMP(a, b) ARR(a) > ARR(b)\n"
			"  #define ASWAP(a, b) tmp = ARR(b); ARR(b) = ARR(a); ARR(a) = tmp;\n"
			"  #define ACSWAP(a, b) if(ACMP(a, b)) { ASWAP(a, b) }\n"
			"  float test0 = 1.2;\n"
			"  float test1 = 0.34;\n"
			"  float test3 = 0.8;\n"
			"  float test4 = 2.9;\n"
			"  float tmp;\n"
			"  ACSWAP(0,2)\n"
			"  ACSWAP(1,3)\n"
			"  ACSWAP(0,1)\n"
			"  ACSWAP(2,3)\n"
			"  ACSWAP(1,2)\n"
			"}\n");
	String expected(
			"fragment() {\n"
			"  float test0 = 1.2;\n"
			"  float test1 = 0.34;\n"
			"  float test3 = 0.8;\n"
			"  float test4 = 2.9;\n"
			"  float tmp;\n"
			"  if(test0 > test2) { tmp = test2; test2 = test0; test0 = tmp; }\n"
			"  if(test1 > test3) { tmp = test3; test3 = test1; test1 = tmp; }\n"
			"  if(test0 > test1) { tmp = test1; test1 = test0; test0 = tmp; }\n"
			"  if(test2 > test3) { tmp = test3; test3 = test2; test2 = tmp; }\n"
			"  if(test1 > test2) { tmp = test2; test2 = test1; test1 = tmp; }\n"
			"}\n");
	String result;

	ShaderPreprocessor preprocessor;
	CHECK_EQ(preprocessor.preprocess(code, String("file.gdshader"), result), Error::OK);

	CHECK_SHADER_EQ(result, expected);
}

TEST_CASE("[ShaderPreprocessor] Undefined behavior") {
	// None of these are valid concatenation, nor valid shader code.
	// Don't care about results, just make sure there's no crash.
	const String filename("somefile.gdshader");
	String result;
	ShaderPreprocessor preprocessor;

	preprocessor.preprocess("#define X ###\nX\n", filename, result);
	preprocessor.preprocess("#define X ####\nX\n", filename, result);
	preprocessor.preprocess("#define X #####\nX\n", filename, result);
	preprocessor.preprocess("#define X 1 ### 2\nX\n", filename, result);
	preprocessor.preprocess("#define X 1 #### 2\nX\n", filename, result);
	preprocessor.preprocess("#define X 1 ##### 2\nX\n", filename, result);
	preprocessor.preprocess("#define X ### 2\nX\n", filename, result);
	preprocessor.preprocess("#define X #### 2\nX\n", filename, result);
	preprocessor.preprocess("#define X ##### 2\nX\n", filename, result);
	preprocessor.preprocess("#define X 1 ###\nX\n", filename, result);
	preprocessor.preprocess("#define X 1 ####\nX\n", filename, result);
	preprocessor.preprocess("#define X 1 #####\nX\n", filename, result);
}

TEST_CASE("[ShaderPreprocessor] Invalid concatenations") {
	const String filename("somefile.gdshader");
	String result;
	ShaderPreprocessor preprocessor;

	CHECK_NE(preprocessor.preprocess("#define X ##", filename, result), Error::OK);
	CHECK_NE(preprocessor.preprocess("#define X 1 ##", filename, result), Error::OK);
	CHECK_NE(preprocessor.preprocess("#define X ## 1", filename, result), Error::OK);
	CHECK_NE(preprocessor.preprocess("#define X(y)   ##  ", filename, result), Error::OK);
	CHECK_NE(preprocessor.preprocess("#define X(y) y ##  ", filename, result), Error::OK);
	CHECK_NE(preprocessor.preprocess("#define X(y) ## y", filename, result), Error::OK);
}

} // namespace TestShaderPreprocessor
