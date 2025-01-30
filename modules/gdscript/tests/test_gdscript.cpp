/**************************************************************************/
/*  test_gdscript.cpp                                                     */
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

#include "test_gdscript.h"

#include "../gdscript_analyzer.h"
#include "../gdscript_compiler.h"
#include "../gdscript_parser.h"
#include "../gdscript_tokenizer.h"
#include "../gdscript_tokenizer_buffer.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"
#include "core/string/string_builder.h"
#include "scene/resources/packed_scene.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

namespace GDScriptTests {

static void test_tokenizer(const String &p_code, const Vector<String> &p_lines) {
	GDScriptTokenizerText tokenizer;
	tokenizer.set_source_code(p_code);

	int tab_size = 4;
#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton()) {
		tab_size = EditorSettings::get_singleton()->get_setting("text_editor/behavior/indent/size");
	}
#endif // TOOLS_ENABLED
	String tab = String(" ").repeat(tab_size);

	GDScriptTokenizer::Token current = tokenizer.scan();
	while (current.type != GDScriptTokenizer::Token::TK_EOF) {
		StringBuilder token;
		token += " --> "; // Padding for line number.

		for (int l = current.start_line; l <= current.end_line && l <= p_lines.size(); l++) {
			print_line(vformat("%04d %s", l, p_lines[l - 1]).replace("\t", tab));
		}

		{
			// Print carets to point at the token.
			StringBuilder pointer;
			pointer += "     "; // Padding for line number.
			int rightmost_column = current.rightmost_column;
			if (current.end_line > current.start_line) {
				rightmost_column--; // Don't point to the newline as a column.
			}
			for (int col = 1; col < rightmost_column; col++) {
				if (col < current.leftmost_column) {
					pointer += " ";
				} else {
					pointer += "^";
				}
			}
			print_line(pointer.as_string());
		}

		token += current.get_name();

		if (current.type == GDScriptTokenizer::Token::ERROR || current.type == GDScriptTokenizer::Token::LITERAL || current.type == GDScriptTokenizer::Token::IDENTIFIER || current.type == GDScriptTokenizer::Token::ANNOTATION) {
			token += "(";
			token += Variant::get_type_name(current.literal.get_type());
			token += ") ";
			token += current.literal;
		}

		print_line(token.as_string());

		print_line("-------------------------------------------------------");

		current = tokenizer.scan();
	}

	print_line(current.get_name()); // Should be EOF
}

static void test_tokenizer_buffer(const Vector<uint8_t> &p_buffer, const Vector<String> &p_lines);

static void test_tokenizer_buffer(const String &p_code, const Vector<String> &p_lines) {
	Vector<uint8_t> binary = GDScriptTokenizerBuffer::parse_code_string(p_code, GDScriptTokenizerBuffer::COMPRESS_NONE);
	test_tokenizer_buffer(binary, p_lines);
}

static void test_tokenizer_buffer(const Vector<uint8_t> &p_buffer, const Vector<String> &p_lines) {
	GDScriptTokenizerBuffer tokenizer;
	tokenizer.set_code_buffer(p_buffer);

	int tab_size = 4;
#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton()) {
		tab_size = EditorSettings::get_singleton()->get_setting("text_editor/behavior/indent/size");
	}
#endif // TOOLS_ENABLED
	String tab = String(" ").repeat(tab_size);

	GDScriptTokenizer::Token current = tokenizer.scan();
	while (current.type != GDScriptTokenizer::Token::TK_EOF) {
		StringBuilder token;
		token += " --> "; // Padding for line number.

		for (int l = current.start_line; l <= current.end_line && l <= p_lines.size(); l++) {
			print_line(vformat("%04d %s", l, p_lines[l - 1]).replace("\t", tab));
		}

		token += current.get_name();

		if (current.type == GDScriptTokenizer::Token::ERROR || current.type == GDScriptTokenizer::Token::LITERAL || current.type == GDScriptTokenizer::Token::IDENTIFIER || current.type == GDScriptTokenizer::Token::ANNOTATION) {
			token += "(";
			token += Variant::get_type_name(current.literal.get_type());
			token += ") ";
			token += current.literal;
		}

		print_line(token.as_string());

		print_line("-------------------------------------------------------");

		current = tokenizer.scan();
	}

	print_line(current.get_name()); // Should be EOF
}

static void test_parser(const String &p_code, const String &p_script_path, const Vector<String> &p_lines) {
	GDScriptParser parser;
	Error err = parser.parse(p_code, p_script_path, false);

	if (err != OK) {
		const List<GDScriptParser::ParserError> &errors = parser.get_errors();
		for (const GDScriptParser::ParserError &error : errors) {
			print_line(vformat("%02d:%02d: %s", error.line, error.column, error.message));
		}
	}

	GDScriptAnalyzer analyzer(&parser);
	err = analyzer.analyze();

	if (err != OK) {
		const List<GDScriptParser::ParserError> &errors = parser.get_errors();
		for (const GDScriptParser::ParserError &error : errors) {
			print_line(vformat("%02d:%02d: %s", error.line, error.column, error.message));
		}
	}

#ifdef TOOLS_ENABLED
	GDScriptParser::TreePrinter printer;
	printer.print_tree(parser);
#endif
}

static void recursively_disassemble_functions(const Ref<GDScript> script, const Vector<String> &p_lines) {
	for (const KeyValue<StringName, GDScriptFunction *> &E : script->get_member_functions()) {
		const GDScriptFunction *func = E.value;

		const MethodInfo &mi = func->get_method_info();
		String signature = "Disassembling " + mi.name + "(";
		for (List<PropertyInfo>::ConstIterator arg_itr = mi.arguments.begin(); arg_itr != mi.arguments.end(); ++arg_itr) {
			if (arg_itr != mi.arguments.begin()) {
				signature += ", ";
			}
			signature += arg_itr->name;
		}
		print_line(signature + ")");
#ifdef TOOLS_ENABLED
		func->disassemble(p_lines);
#endif
		print_line("");
		print_line("");
	}

	for (const KeyValue<StringName, Ref<GDScript>> &F : script->get_subclasses()) {
		const Ref<GDScript> inner_script = F.value;
		print_line("");
		print_line(vformat("Inner Class: %s", inner_script->get_local_name()));
		print_line("");
		recursively_disassemble_functions(inner_script, p_lines);
	}
}

static void test_compiler(const String &p_code, const String &p_script_path, const Vector<String> &p_lines) {
	GDScriptParser parser;
	Error err = parser.parse(p_code, p_script_path, false);

	if (err != OK) {
		print_line("Error in parser:");
		const List<GDScriptParser::ParserError> &errors = parser.get_errors();
		for (const GDScriptParser::ParserError &error : errors) {
			print_line(vformat("%02d:%02d: %s", error.line, error.column, error.message));
		}
		return;
	}

	GDScriptAnalyzer analyzer(&parser);
	err = analyzer.analyze();

	if (err != OK) {
		print_line("Error in analyzer:");
		const List<GDScriptParser::ParserError> &errors = parser.get_errors();
		for (const GDScriptParser::ParserError &error : errors) {
			print_line(vformat("%02d:%02d: %s", error.line, error.column, error.message));
		}
		return;
	}

	GDScriptCompiler compiler;
	Ref<GDScript> script;
	script.instantiate();
	script->set_path(p_script_path);

	err = compiler.compile(&parser, script.ptr(), false);

	if (err) {
		print_line("Error in compiler:");
		print_line(vformat("%02d:%02d: %s", compiler.get_error_line(), compiler.get_error_column(), compiler.get_error()));
		return;
	}

	recursively_disassemble_functions(script, p_lines);
}

void test(TestType p_type) {
	List<String> cmdlargs = OS::get_singleton()->get_cmdline_args();

	if (cmdlargs.is_empty()) {
		return;
	}

	String test = cmdlargs.back()->get();
	if (!test.ends_with(".gd") && !test.ends_with(".gdc")) {
		print_line("This test expects a path to a GDScript file as its last parameter. Got: " + test);
		return;
	}

	Ref<FileAccess> fa = FileAccess::open(test, FileAccess::READ);
	ERR_FAIL_COND_MSG(fa.is_null(), "Could not open file: " + test);

	// Initialize the language for the test routine.
	init_language(fa->get_path_absolute().get_base_dir());

	// Load global classes.
	TypedArray<Dictionary> script_classes = ProjectSettings::get_singleton()->get_global_class_list();
	for (int i = 0; i < script_classes.size(); i++) {
		Dictionary c = script_classes[i];
		if (!c.has("class") || !c.has("language") || !c.has("path") || !c.has("base") || !c.has("is_abstract")) {
			continue;
		}
		ScriptServer::add_global_class(c["class"], c["base"], c["language"], c["path"], c["is_abstract"]);
	}

	Vector<uint8_t> buf;
	uint64_t flen = fa->get_length();
	buf.resize(flen + 1);
	fa->get_buffer(buf.ptrw(), flen);
	buf.write[flen] = 0;

	String code;
	code.parse_utf8((const char *)&buf[0]);

	Vector<String> lines;
	int last = 0;
	for (int i = 0; i <= code.length(); i++) {
		if (code[i] == '\n' || code[i] == 0) {
			lines.push_back(code.substr(last, i - last));
			last = i + 1;
		}
	}

	switch (p_type) {
		case TEST_TOKENIZER:
			test_tokenizer(code, lines);
			break;
		case TEST_TOKENIZER_BUFFER:
			if (test.ends_with(".gdc")) {
				test_tokenizer_buffer(buf, lines);
			} else {
				test_tokenizer_buffer(code, lines);
			}
			break;
		case TEST_PARSER:
			test_parser(code, test, lines);
			break;
		case TEST_COMPILER:
			test_compiler(code, test, lines);
			break;
		case TEST_BYTECODE:
			print_line("Not implemented.");
	}

	finish_language();
}
} // namespace GDScriptTests
