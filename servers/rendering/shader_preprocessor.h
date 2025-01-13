/**************************************************************************/
/*  shader_preprocessor.h                                                 */
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

#ifndef SHADER_PREPROCESSOR_H
#define SHADER_PREPROCESSOR_H

#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/templates/local_vector.h"
#include "core/templates/rb_map.h"
#include "core/templates/rb_set.h"
#include "core/typedefs.h"

#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "scene/resources/shader.h"
#include "scene/resources/shader_include.h"

class ShaderPreprocessor {
public:
	enum CompletionType {
		COMPLETION_TYPE_NONE,
		COMPLETION_TYPE_DIRECTIVE,
		COMPLETION_TYPE_PRAGMA_DIRECTIVE,
		COMPLETION_TYPE_PRAGMA,
		COMPLETION_TYPE_CONDITION,
		COMPLETION_TYPE_INCLUDE_PATH,
	};

	struct FilePosition {
		String file;
		int line = 0;
	};

	struct Region {
		String file;
		int from_line = -1;
		int to_line = -1;
		bool enabled = false;
		Region *parent = nullptr;
	};

private:
	struct Token {
		char32_t text;
		int line;

		Token();
		Token(char32_t p_text, int p_line);
	};

	// The real preprocessor that understands basic shader and preprocessor language syntax.
	class Tokenizer {
	public:
		String code;
		int line;
		int index;
		int size;
		LocalVector<Token> generated;

	private:
		void add_generated(const Token &p_t);
		char32_t next();

	public:
		int get_line() const;
		int get_index() const;
		char32_t peek();
		int consume_line_continuations(int p_offset);

		void get_and_clear_generated(LocalVector<char32_t> *r_out);
		void backtrack(char32_t p_what);
		LocalVector<Token> advance(char32_t p_what);
		void skip_whitespace();
		bool consume_empty_line();
		String get_identifier(bool *r_is_cursor = nullptr, bool p_started = false);
		String peek_identifier();
		Token get_token();

		Tokenizer(const String &p_code);
	};

	class CommentRemover {
	private:
		LocalVector<char32_t> stripped;
		String code;
		int index;
		int line;
		int comment_line_open;
		int comments_open;
		int strings_open;

	public:
		String get_error() const;
		int get_error_line() const;
		char32_t peek() const;

		bool advance(char32_t p_what);
		String strip();

		CommentRemover(const String &p_code);
	};

	struct Define {
		Vector<String> arguments;
		String body;
		bool is_builtin = false;
	};

	struct Branch {
		Vector<bool> conditions;
		Branch *parent = nullptr;
		bool else_defined = false;

		Branch() {}

		Branch(bool p_condition, Branch *p_parent) :
				parent(p_parent) {
			conditions.push_back(p_condition);
		}
	};

	struct State {
		RBMap<String, Define *> defines;
		List<Branch> branches;
		Branch *current_branch = nullptr;
		int condition_depth = 0;
		RBSet<String> includes;
		List<uint64_t> cyclic_include_hashes; // Holds code hash of includes.
		int include_depth = 0;
		String current_filename;
		String current_shader_type;
		String error;
		List<FilePosition> include_positions;
		bool save_regions = false;
		RBMap<String, List<Region>> regions;
		Region *previous_region = nullptr;
		bool disabled = false;
		CompletionType completion_type = COMPLETION_TYPE_NONE;
		HashSet<Ref<ShaderInclude>> shader_includes;
	};

private:
	LocalVector<char32_t> output;
	State *state = nullptr;

private:
	static bool is_char_word(char32_t p_char);
	static bool is_char_space(char32_t p_char);
	static bool is_char_end(char32_t p_char);
	static String vector_to_string(const LocalVector<char32_t> &p_v, int p_start = 0, int p_end = -1);
	static String tokens_to_string(const LocalVector<Token> &p_tokens);

	void _set_expected_error(const String &p_what, int p_line) {
		set_error(vformat(RTR("Expected a '%s'."), p_what), p_line);
	}

	void _set_unexpected_token_error(const String &p_what, int p_line) {
		set_error(vformat(RTR("Unexpected token: '%s'."), p_what), p_line);
	}

	void process_directive(Tokenizer *p_tokenizer);
	void process_define(Tokenizer *p_tokenizer);
	void process_elif(Tokenizer *p_tokenizer);
	void process_else(Tokenizer *p_tokenizer);
	void process_endif(Tokenizer *p_tokenizer);
	void process_error(Tokenizer *p_tokenizer);
	void process_if(Tokenizer *p_tokenizer);
	void process_ifdef(Tokenizer *p_tokenizer);
	void process_ifndef(Tokenizer *p_tokenizer);
	void process_include(Tokenizer *p_tokenizer);
	void process_pragma(Tokenizer *p_tokenizer);
	void process_undef(Tokenizer *p_tokenizer);

	void add_region(int p_line, bool p_enabled, Region *p_parent_region);
	void start_branch_condition(Tokenizer *p_tokenizer, bool p_success, bool p_continue = false);

	Error expand_condition(const String &p_string, int p_line, String &r_result);
	void expand_output_macros(int p_start, int p_line);
	Error expand_macros(const String &p_string, int p_line, String &r_result);
	bool expand_macros_once(const String &p_line, int p_line_number, const RBMap<String, Define *>::Element *p_define_pair, String &r_expanded);
	bool find_match(const String &p_string, const String &p_value, int &r_index, int &r_index_start);
	void concatenate_macro_body(String &r_body);

	String next_directive(Tokenizer *p_tokenizer, const Vector<String> &p_directives);
	void add_to_output(const String &p_str);
	void set_error(const String &p_error, int p_line);

	static Define *create_define(const String &p_body);
	void insert_builtin_define(String p_name, String p_value, State &p_state);

	void clear_state();

	Error preprocess(State *p_state, const String &p_code, String &r_result);

public:
	typedef void (*IncludeCompletionFunction)(List<ScriptLanguage::CodeCompletionOption> *);

	Error preprocess(const String &p_code, const String &p_filename, String &r_result, String *r_error_text = nullptr, List<FilePosition> *r_error_position = nullptr, List<Region> *r_regions = nullptr, HashSet<Ref<ShaderInclude>> *r_includes = nullptr, List<ScriptLanguage::CodeCompletionOption> *r_completion_options = nullptr, List<ScriptLanguage::CodeCompletionOption> *r_completion_defines = nullptr, IncludeCompletionFunction p_include_completion_func = nullptr);

	static void get_keyword_list(List<String> *r_keywords, bool p_include_shader_keywords, bool p_ignore_context_keywords = false);
	static void get_pragma_list(List<String> *r_pragmas);

	ShaderPreprocessor();
	~ShaderPreprocessor();
};

#endif // SHADER_PREPROCESSOR_H
