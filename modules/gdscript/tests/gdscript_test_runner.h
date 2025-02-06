/**************************************************************************/
/*  gdscript_test_runner.h                                                */
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

#include "../gdscript.h"

#include "core/error/error_macros.h"
#include "core/string/print_string.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

namespace GDScriptTests {

void init_autoloads();
void init_language(const String &p_base_path);
void finish_language();

// Single test instance in a suite.
class GDScriptTest {
public:
	enum TestStatus {
		GDTEST_OK,
		GDTEST_LOAD_ERROR,
		GDTEST_PARSER_ERROR,
		GDTEST_ANALYZER_ERROR,
		GDTEST_COMPILER_ERROR,
		GDTEST_RUNTIME_ERROR,
	};

	struct TestResult {
		TestStatus status;
		String output;
		bool passed;
	};

	enum TokenizerMode {
		TOKENIZER_TEXT,
		TOKENIZER_BUFFER,
	};

private:
	struct ErrorHandlerData {
		TestResult *result = nullptr;
		GDScriptTest *self = nullptr;
		ErrorHandlerData(TestResult *p_result, GDScriptTest *p_this) {
			result = p_result;
			self = p_this;
		}
	};

	String source_file;
	String output_file;
	String base_dir;

	PrintHandlerList _print_handler;
	ErrorHandlerList _error_handler;

	TokenizerMode tokenizer_mode = TOKENIZER_TEXT;

	void enable_stdout();
	void disable_stdout();
	bool check_output(const String &p_output) const;
	String get_text_for_status(TestStatus p_status) const;

	TestResult execute_test_code(bool p_is_generating);

public:
	static void print_handler(void *p_this, const String &p_message, bool p_error, bool p_rich);
	static void error_handler(void *p_this, const char *p_function, const char *p_file, int p_line, const char *p_error, const char *p_explanation, bool p_editor_notify, ErrorHandlerType p_type);
	TestResult run_test();
	bool generate_output();

	const String &get_source_file() const { return source_file; }
	const String get_source_relative_filepath() const { return source_file.trim_prefix(base_dir); }
	const String &get_output_file() const { return output_file; }

	void set_tokenizer_mode(TokenizerMode p_tokenizer_mode) { tokenizer_mode = p_tokenizer_mode; }
	TokenizerMode get_tokenizer_mode() const { return tokenizer_mode; }

	GDScriptTest(const String &p_source_path, const String &p_output_path, const String &p_base_dir);
	GDScriptTest() :
			GDScriptTest(String(), String(), String()) {} // Needed to use in Vector.
};

class GDScriptTestRunner {
	String source_dir;
	Vector<GDScriptTest> tests;

	bool is_generating = false;
	bool do_init_languages = false;
	bool print_filenames; // Whether filenames should be printed when generated/running tests
	bool binary_tokens; // Test with buffer tokenizer.

	bool make_tests();
	bool make_tests_for_dir(const String &p_dir);
	bool generate_class_index();

public:
	static StringName test_function_name;

	static void handle_cmdline();
	int run_tests();
	bool generate_outputs();

	GDScriptTestRunner(const String &p_source_dir, bool p_init_language, bool p_print_filenames = false, bool p_use_binary_tokens = false);
	~GDScriptTestRunner();
};

} // namespace GDScriptTests
