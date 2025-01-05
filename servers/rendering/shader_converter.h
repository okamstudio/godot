/**************************************************************************/
/*  shader_converter.h                                                    */
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

#ifndef SHADER_CONVERTER_H
#define SHADER_CONVERTER_H

#ifndef DISABLE_DEPRECATED

#include "core/templates/pair.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering_server.h"

class ShaderDeprecatedConverter {
public:
	using TokenType = ShaderLanguage::TokenType;
	using Token = ShaderLanguage::Token;
	using TT = TokenType;
	using TokenE = List<Token>::Element;

	ShaderDeprecatedConverter() {}
	bool is_code_deprecated(const String &p_code);
	bool convert_code(const String &p_code);
	String get_error_text() const;
	int get_error_line() const;
	String emit_code() const;
	void set_warning_comments(bool p_add_comments);
	void set_fail_on_unported(bool p_fail_on_unported);
	void set_assume_correct(bool p_assume_correct);
	void set_force_reserved_word_replacement(bool p_force_reserved_word_replacement);
	void set_verbose_comments(bool p_verbose_comments);
	String get_report();

	static bool tokentype_is_identifier(const TokenType &p_tk_type);
	static bool tokentype_is_new_reserved_keyword(const TokenType &p_tk_type);
	static bool tokentype_is_new_type(const TokenType &p_tk_type);
	static bool tokentype_is_new_hint(const TokenType &p_tk);

	static bool id_is_new_builtin_func(const String &p_name);

	static String get_tokentype_text(TokenType p_tk_type);
	static TokenType get_tokentype_from_text(const String &p_text);

	static bool has_builtin_rename(RS::ShaderMode p_mode, const String &p_name, const String &p_function = "");
	static String get_builtin_rename(const String &p_name);

	static bool has_hint_replacement(const String &p_name);
	static TokenType get_hint_replacement(const String &p_name);

	static bool is_renamed_render_mode(RS::ShaderMode p_mode, const String &p_name);
	static String get_render_mode_rename(const String &p_name);

	static bool has_removed_render_mode(RS::ShaderMode p_mode, const String &p_name);
	static bool can_remove_render_mode(const String &p_name);

	static bool has_removed_type(const String &p_name);

	static bool is_renamed_main_function(RS::ShaderMode p_mode, const String &p_name);
	static bool is_renamee_main_function(RS::ShaderMode p_mode, const String &p_name);
	static String get_main_function_rename(const String &p_name);
	static TokenType get_renamed_function_type(const String &p_name);
	static int get_renamed_function_arg_count(const String &p_name);

	static bool is_removed_builtin(RS::ShaderMode p_mode, const String &p_name, const String &p_function = "");
	static TokenType get_removed_builtin_uniform_type(const String &p_name);
	static Vector<TokenType> get_removed_builtin_hints(const String &p_name);

	static bool _rename_has_special_handling(const String &p_name);

	static void _get_builtin_renames_list(List<String> *r_list);
	static void _get_render_mode_renames_list(List<String> *r_list);
	static void _get_hint_renames_list(List<String> *r_list);
	static void _get_function_renames_list(List<String> *r_list);
	static void _get_render_mode_removals_list(List<String> *r_list);
	static void _get_builtin_removals_list(List<String> *r_list);
	static void _get_type_removals_list(List<String> *r_list);
	static void _get_new_builtin_funcs_list(List<String> *r_list);
	static Vector<String> _get_funcs_builtin_rename(RS::ShaderMode p_mode, const String &p_name);
	static Vector<String> _get_funcs_builtin_removal(RS::ShaderMode p_mode, const String &p_name);

	struct RenamedBuiltins {
		const char *name;
		const char *replacement;
		const Vector<Pair<RS::ShaderMode, Vector<String>>> mode_functions;
		const bool special_handling;
	};

	struct RenamedRenderModes {
		const RS::ShaderMode mode;
		const char *name;
		const char *replacement;
	};

	struct RenamedHints {
		const char *name;
		const ShaderLanguage::TokenType replacement;
	};

	struct RenamedFunctions {
		const RS::ShaderMode mode;
		const ShaderLanguage::TokenType type;
		const int arg_count;
		const char *name;
		const char *replacement;
	};

	struct RemovedRenderModes {
		const RS::ShaderMode mode;
		const char *name;
		const bool can_remove;
	};

	struct RemovedBuiltins {
		const char *name;
		const ShaderLanguage::TokenType uniform_type;
		const Vector<ShaderLanguage::TokenType> hints;
		const Vector<Pair<RS::ShaderMode, Vector<String>>> mode_functions;
	};

private:
	struct UniformDecl {
		List<Token>::Element *start_pos = nullptr;
		List<Token>::Element *uniform_stmt_pos = nullptr;
		List<Token>::Element *end_pos = nullptr;
		List<Token>::Element *interp_qual_pos = nullptr;
		List<Token>::Element *type_pos = nullptr;
		List<Token>::Element *name_pos = nullptr;
		Vector<List<Token>::Element *> hint_poses;

		bool is_array = false;
		bool has_uniform_qual() const {
			return start_pos != nullptr && ShaderLanguage::is_token_uniform_qual(start_pos->get().type);
		}
		bool has_interp_qual() const {
			return interp_qual_pos != nullptr;
		}
	};
	struct VarDecl {
		List<Token>::Element *start_pos = nullptr;
		List<Token>::Element *end_pos = nullptr; // semicolon or comma or right paren
		List<Token>::Element *type_pos = nullptr;
		List<Token>::Element *name_pos = nullptr;
		bool is_array = false;
		bool new_arr_style_decl = false;
		bool is_func_arg = false;
		void clear() {
			start_pos = nullptr;
			end_pos = nullptr;
			type_pos = nullptr;
			name_pos = nullptr;
		}
	};

	struct StructDecl {
		List<Token>::Element *start_pos = nullptr;
		List<Token>::Element *name_pos = nullptr;
		List<Token>::Element *body_start_pos = nullptr; // left curly
		List<Token>::Element *body_end_pos = nullptr; // right curly - end of struct
		HashMap<String, VarDecl> members;
		void clear() {
			start_pos = nullptr;
			name_pos = nullptr;
			body_start_pos = nullptr;
			body_end_pos = nullptr;
		}
	};

	struct FunctionDecl {
		List<Token>::Element *start_pos = nullptr;
		List<Token>::Element *type_pos = nullptr;
		List<Token>::Element *name_pos = nullptr;
		List<Token>::Element *args_start_pos = nullptr; // left paren
		List<Token>::Element *args_end_pos = nullptr; // right paren
		List<Token>::Element *body_start_pos = nullptr; // left curly
		List<Token>::Element *body_end_pos = nullptr; // right curly - end of function
		bool is_renamed_main_function(RS::ShaderMode p_mode) const;
		bool is_new_main_function(RS::ShaderMode p_mode) const;

		int arg_count = 0;
		bool has_array_return_type = false;
		void clear() {
			type_pos = nullptr;
			name_pos = nullptr;
			args_start_pos = nullptr;
			args_end_pos = nullptr;
			body_start_pos = nullptr;
			body_end_pos = nullptr;
		}
	};
	static const RenamedBuiltins renamed_builtins[];
	static const RenamedRenderModes renamed_render_modes[];
	static const RenamedHints renamed_hints[];
	static const RenamedFunctions renamed_functions[];
	static const RemovedRenderModes removed_render_modes[];
	static const RemovedBuiltins removed_builtins[];
	static const char *removed_types[];
	static const char *old_builtin_funcs[];
	static HashSet<String> _new_builtin_funcs;
	String old_code;
	List<Token> code_tokens;
	List<Token>::Element *curr_ptr = nullptr;
	List<Token>::Element *after_shader_decl = nullptr;

	HashMap<String, UniformDecl> uniform_decls;
	HashMap<String, Vector<VarDecl>> var_decls;
	HashMap<String, FunctionDecl> function_decls;
	HashMap<String, HashSet<String>> scope_declarations;

	HashMap<String, StructDecl> struct_decls;
	RenderingServer::ShaderMode shader_mode = RenderingServer::ShaderMode::SHADER_MAX;

	bool warning_comments = true;
	bool verbose_comments = false;
	bool fail_on_unported = true;

	bool function_pass_failed = false;
	bool var_pass_failed = false;
	String err_str;
	int err_line = 0;

	Token eof_token{ ShaderLanguage::TK_EOF, {}, 0, 0, 0, 0 };

	HashMap<int, Vector<String>> report;

	static RS::ShaderMode get_shader_mode_from_string(const String &p_mode);

	String get_token_literal_text(const Token &p_tk) const;
	static Token mkTok(TokenType p_type, const StringName &p_text = StringName(), double constant = 0, uint16_t p_line = 0);
	static bool token_is_skippable(const Token &p_tk);
	bool token_is_type(const Token &p_tk);
	static bool token_is_hint(const Token &p_tk);

	void reset();
	bool _preprocess_code();
	List<Token>::Element *get_next_token();
	List<Token>::Element *get_prev_token();
	List<Token>::Element *remove_cur_and_get_next();
	TokenType peek_next_tk_type(uint32_t p_count = 1) const;
	TokenType peek_prev_tk_type(uint32_t p_count = 1) const;
	List<Token>::Element *get_pos() const;
	bool reset_to(List<Token>::Element *p_pos);
	bool insert_after(const Vector<Token> &p_token_list, List<Token>::Element *p_pos);
	bool insert_before(const Vector<Token> &p_token_list, List<Token>::Element *p_pos);
	bool insert_after(const Token &p_token, List<Token>::Element *p_pos);
	bool insert_before(const Token &p_token, List<Token>::Element *p_pos);
	List<Token>::Element *replace_curr(const Token &p_token, String comment_prefix = String());
	List<Token>::Element *_get_next_token_ptr(List<Token>::Element *p_curr_ptr) const;
	List<Token>::Element *_get_prev_token_ptr(List<Token>::Element *p_curr_ptr) const;
	TokenType _peek_tk_type(int64_t p_count, List<Token>::Element **r_pos = nullptr) const;

	bool scope_has_decl(const String &p_scope, const String &p_name) const;
	bool _handle_new_keyword_rename(TokenE *pos, bool p_detected_3x, HashMap<TokenType, String> &p_func_renames);
	TokenE *_rename_keyword_id(TokenE *pos, bool p_detected_3x, const HashMap<TokenType, String> &p_func_renames);

	bool _has_any_preprocessor_directives();
	bool _is_code_deprecated();
	bool _parse_uniform();
	bool _tok_is_start_of_decl(const Token &p_tk);
	bool _skip_uniform();
	bool _parse_uniforms();
	bool _parse_structs();
	bool _skip_array_size();
	bool _parse_struct();
	bool _skip_struct();
	bool _check_deprecated_type(TokenE *p_type_tok);
	bool _add_to_report(int p_line, const String &p_msg, int level = 0);
	bool _add_comment_before(const String &p_comment, List<Token>::Element *p_pos, bool warning = true);
	bool _add_comment_at_eol(const String &p_comment, List<Token>::Element *p_pos);
	bool _process_func_decl_statement(TokenE *p_start_tok, TokenE *p_type_tok, bool p_second_pass = false);
	bool _process_decl_statement(TokenE *p_start_tok, TokenE *p_type_tok, const String &p_scope = "<global>", bool p_func_args = false);
	bool _parse_decls(bool p_first_pass);
	bool _process_decl_if_exist(String p_curr_func, bool p_first_pass);
	bool _insert_uniform_declaration(const String &p_name);
	List<Token>::Element *_remove_from_curr_to(List<Token>::Element *p_end);
	List<Token>::Element *_get_end_of_closure();
	static HashSet<String> _construct_new_builtin_funcs();

	enum {
		NEW_IDENT = -1
	};
};
#endif // DISABLE_DEPRECATED

#endif // SHADER_CONVERTER_H
