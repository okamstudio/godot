/**************************************************************************/
/*  gdscript_highlighter.cpp                                              */
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

#include "gdscript_highlighter.h"

#include "../gdscript.h"
#include "../gdscript_tokenizer.h"

#include "core/config/project_settings.h"
#include "editor/editor_settings.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/text_edit.h"

Dictionary GDScriptSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	Dictionary color_map;

	Type next_type = NONE;
	Type current_type = NONE;
	Type prev_type = NONE;

	String prev_text = "";
	int prev_column = 0;
	bool prev_is_char = false;
	bool prev_is_digit = false;
	bool prev_is_binary_op = false;

	bool in_keyword = false;
	bool in_word = false;
	bool in_number = false;
	bool in_node_path = false;
	bool in_node_ref = false;
	bool in_annotation = false;
	bool in_string_name = false;
	bool is_hex_notation = false;
	bool is_bin_notation = false;
	bool in_member_variable = false;
	bool in_lambda = false;

	bool in_function_name = false; // Any call.
	bool in_function_declaration = false; // Only declaration.
	bool in_signal_declaration = false;
	bool is_after_func_signal_declaration = false;
	bool in_var_const_declaration = false;
	bool is_after_var_const_declaration = false;
	bool expect_type = false;

	int in_declaration_params = 0; // The number of opened `(` after func/signal name.
	int in_declaration_param_dicts = 0; // The number of opened `{` inside func params.
	int in_type_params = 0; // The number of opened `[` after type name.

	Color keyword_color;
	Color color;

	color_region_cache[p_line] = -1;
	int in_region = -1;
	if (p_line != 0) {
		int prev_region_line = p_line - 1;
		while (prev_region_line > 0 && !color_region_cache.has(prev_region_line)) {
			prev_region_line--;
		}
		for (int i = prev_region_line; i < p_line - 1; i++) {
			get_line_syntax_highlighting(i);
		}
		if (!color_region_cache.has(p_line - 1)) {
			get_line_syntax_highlighting(p_line - 1);
		}
		in_region = color_region_cache[p_line - 1];
	}

	const String &str = text_edit->get_line(p_line);
	const int line_length = str.length();
	Color prev_color;

	if (in_region != -1 && line_length == 0) {
		color_region_cache[p_line] = in_region;
	}
	for (int j = 0; j < line_length; j++) {
		Dictionary highlighter_info;

		color = font_color;
		bool is_char = !is_symbol(str[j]);
		bool is_a_symbol = is_symbol(str[j]);
		bool is_a_digit = is_digit(str[j]);
		bool is_binary_op = false;

		/* color regions */
		if (is_a_symbol || in_region != -1) {
			int from = j;

			if (in_region == -1) {
				for (; from < line_length; from++) {
					if (str[from] == '\\') {
						from++;
						continue;
					}
					break;
				}
			}

			if (from != line_length) {
				// Check if we are in entering a region.
				if (in_region == -1) {
					const bool r_prefix = from > 0 && str[from - 1] == 'r';
					for (int c = 0; c < color_regions.size(); c++) {
						// Check there is enough room.
						int chars_left = line_length - from;
						int start_key_length = color_regions[c].start_key.length();
						int end_key_length = color_regions[c].end_key.length();
						if (chars_left < start_key_length) {
							continue;
						}

						if (color_regions[c].is_string && color_regions[c].r_prefix != r_prefix) {
							continue;
						}

						// Search the line.
						bool match = true;
						const char32_t *start_key = color_regions[c].start_key.get_data();
						for (int k = 0; k < start_key_length; k++) {
							if (start_key[k] != str[from + k]) {
								match = false;
								break;
							}
						}
						if (!match) {
							continue;
						}
						in_region = c;
						from += start_key_length;

						// Check if it's the whole line.
						if (end_key_length == 0 || color_regions[c].line_only || from + end_key_length > line_length) {
							// Don't skip comments, for highlighting markers.
							if (color_regions[in_region].is_comment) {
								break;
							}
							if (from + end_key_length > line_length) {
								// If it's key length and there is a '\', dont skip to highlight esc chars.
								if (str.find_char('\\', from) >= 0) {
									break;
								}
							}
							prev_color = color_regions[in_region].color;
							highlighter_info["color"] = color_regions[c].color;
							color_map[j] = highlighter_info;

							j = line_length;
							if (!color_regions[c].line_only) {
								color_region_cache[p_line] = c;
							}
						}
						break;
					}

					// Don't skip comments, for highlighting markers.
					if (j == line_length && !color_regions[in_region].is_comment) {
						continue;
					}
				}

				// If we are in one, find the end key.
				if (in_region != -1) {
					Color region_color = color_regions[in_region].color;
					if (in_node_path && color_regions[in_region].type == ColorRegion::TYPE_STRING) {
						region_color = node_path_color;
					}
					if (in_node_ref && color_regions[in_region].type == ColorRegion::TYPE_STRING) {
						region_color = node_ref_color;
					}
					if (in_string_name && color_regions[in_region].type == ColorRegion::TYPE_STRING) {
						region_color = string_name_color;
					}

					prev_color = region_color;
					highlighter_info["color"] = region_color;
					color_map[j] = highlighter_info;

					if (color_regions[in_region].is_comment) {
						int marker_start_pos = from;
						int marker_len = 0;
						while (from <= line_length) {
							if (from < line_length && is_unicode_identifier_continue(str[from])) {
								marker_len++;
							} else {
								if (marker_len > 0) {
									HashMap<String, CommentMarkerLevel>::ConstIterator E = comment_markers.find(str.substr(marker_start_pos, marker_len));
									if (E) {
										Dictionary marker_highlighter_info;
										marker_highlighter_info["color"] = comment_marker_colors[E->value];
										color_map[marker_start_pos] = marker_highlighter_info;

										Dictionary marker_continue_highlighter_info;
										marker_continue_highlighter_info["color"] = region_color;
										color_map[from] = marker_continue_highlighter_info;
									}
								}
								marker_start_pos = from + 1;
								marker_len = 0;
							}
							from++;
						}
						from = line_length - 1;
						j = from;
					} else {
						// Search the line.
						int region_end_index = -1;
						int end_key_length = color_regions[in_region].end_key.length();
						const char32_t *end_key = color_regions[in_region].end_key.get_data();
						for (; from < line_length; from++) {
							if (line_length - from < end_key_length) {
								// Don't break if '\' to highlight esc chars.
								if (str.find_char('\\', from) < 0) {
									break;
								}
							}

							if (!is_symbol(str[from])) {
								continue;
							}

							if (str[from] == '\\') {
								if (!color_regions[in_region].r_prefix) {
									Dictionary escape_char_highlighter_info;
									escape_char_highlighter_info["color"] = symbol_color;
									color_map[from] = escape_char_highlighter_info;
								}

								from++;

								if (!color_regions[in_region].r_prefix) {
									int esc_len = 0;
									if (str[from] == 'u') {
										esc_len = 4;
									} else if (str[from] == 'U') {
										esc_len = 6;
									}
									for (int k = 0; k < esc_len && from < line_length - 1; k++) {
										if (!is_hex_digit(str[from + 1])) {
											break;
										}
										from++;
									}

									Dictionary region_continue_highlighter_info;
									region_continue_highlighter_info["color"] = region_color;
									color_map[from + 1] = region_continue_highlighter_info;
								}

								continue;
							}

							region_end_index = from;
							for (int k = 0; k < end_key_length; k++) {
								if (end_key[k] != str[from + k]) {
									region_end_index = -1;
									break;
								}
							}

							if (region_end_index != -1) {
								break;
							}
						}
						j = from + (end_key_length - 1);
						if (region_end_index == -1) {
							color_region_cache[p_line] = in_region;
						}
					}

					prev_type = REGION;
					prev_text = "";
					prev_column = j;

					in_region = -1;
					prev_is_char = false;
					prev_is_digit = false;
					prev_is_binary_op = false;
					continue;
				}
			}
		}

		// VERY hacky... but couldn't come up with anything better.
		if (j > 0 && (str[j] == '&' || str[j] == '^' || str[j] == '%' || str[j] == '+' || str[j] == '-' || str[j] == '~' || str[j] == '.')) {
			int to = j - 1;
			// Find what the last text was (prev_text won't work if there's no whitespace, so we need to do it manually).
			while (to > 0 && is_whitespace(str[to])) {
				to--;
			}
			int from = to;
			while (from > 0 && !is_symbol(str[from])) {
				from--;
			}
			String word = str.substr(from + 1, to - from);
			// Keywords need to be exceptions, except for keywords that represent a value.
			if (word == "true" || word == "false" || word == "null" || word == "PI" || word == "TAU" || word == "INF" || word == "NAN" || word == "self" || word == "super" || !reserved_keywords.has(word)) {
				if (!is_symbol(str[to]) || str[to] == '"' || str[to] == '\'' || str[to] == ')' || str[to] == ']' || str[to] == '}') {
					is_binary_op = true;
				}
			}
		}

		if (!is_char) {
			in_keyword = false;
		}

		// Allow ABCDEF in hex notation.
		if (is_hex_notation && (is_hex_digit(str[j]) || is_a_digit)) {
			is_a_digit = true;
		} else if (str[j] != '_') {
			is_hex_notation = false;
		}

		// Disallow anything not a 0 or 1 in binary notation.
		if (is_bin_notation && !is_binary_digit(str[j])) {
			is_a_digit = false;
			is_bin_notation = false;
		}

		if (!in_number && !in_word && is_a_digit) {
			in_number = true;
		}

		// Special cases for numbers.
		if (in_number && !is_a_digit) {
			if (str[j] == 'b' && str[j - 1] == '0') {
				is_bin_notation = true;
			} else if (str[j] == 'x' && str[j - 1] == '0') {
				is_hex_notation = true;
			} else if (!((str[j] == '-' || str[j] == '+') && str[j - 1] == 'e' && !prev_is_digit) &&
					!(str[j] == '_' && (prev_is_digit || str[j - 1] == 'b' || str[j - 1] == 'x' || str[j - 1] == '.')) &&
					!(str[j] == 'e' && (prev_is_digit || str[j - 1] == '_')) &&
					!(str[j] == '.' && (prev_is_digit || (!prev_is_binary_op && (j > 0 && (str[j - 1] == '_' || str[j - 1] == '-' || str[j - 1] == '+' || str[j - 1] == '~'))))) &&
					!((str[j] == '-' || str[j] == '+' || str[j] == '~') && !is_binary_op && !prev_is_binary_op && str[j - 1] != 'e')) {
				/* This condition continues number highlighting in special cases.
				1st row: '+' or '-' after scientific notation (like 3e-4);
				2nd row: '_' as a numeric separator;
				3rd row: Scientific notation 'e' and floating points;
				4th row: Floating points inside the number, or leading if after a unary mathematical operator;
				5th row: Multiple unary mathematical operators (like ~-7) */
				in_number = false;
			}
		} else if (str[j] == '.' && !is_binary_op && is_digit(str[j + 1]) && (j == 0 || (j > 0 && str[j - 1] != '.'))) {
			// Start number highlighting from leading decimal points (like .42)
			in_number = true;
		} else if ((str[j] == '-' || str[j] == '+' || str[j] == '~') && !is_binary_op) {
			// Only start number highlighting on unary operators if a digit follows them.
			int non_op = j + 1;
			while (str[non_op] == '-' || str[non_op] == '+' || str[non_op] == '~') {
				non_op++;
			}
			if (is_digit(str[non_op]) || (str[non_op] == '.' && non_op < line_length && is_digit(str[non_op + 1]))) {
				in_number = true;
			}
		}

		if (!in_word && is_unicode_identifier_start(str[j]) && !in_number) {
			in_word = true;
		}

		if (is_a_symbol && str[j] != '.' && in_word) {
			in_word = false;
		}

		if (!in_keyword && is_char && !prev_is_char) {
			int to = j;
			while (to < line_length && !is_symbol(str[to])) {
				to++;
			}

			String word = str.substr(j, to - j);
			Color col;
			if (global_functions.has(word)) {
				// "assert" and "preload" are reserved, so highlight even if not followed by a bracket.
				if (word == GDScriptTokenizer::get_token_name(GDScriptTokenizer::Token::ASSERT) || word == GDScriptTokenizer::get_token_name(GDScriptTokenizer::Token::PRELOAD)) {
					col = global_function_color;
				} else {
					// For other global functions, check if followed by bracket.
					int k = to;
					while (k < line_length && is_whitespace(str[k])) {
						k++;
					}

					if (str[k] == '(') {
						col = global_function_color;
					}
				}
			} else if (class_names.has(word)) {
				col = class_names[word];
			} else if (reserved_keywords.has(word)) {
				col = reserved_keywords[word];
				// Don't highlight `list` as a type in `for elem: Type in list`.
				expect_type = false;
			} else if (member_keywords.has(word)) {
				col = member_keywords[word];
			}

			if (col != Color()) {
				for (int k = j - 1; k >= 0; k--) {
					if (str[k] == '.') {
						col = Color(); // Keyword, member & global func indexing not allowed.
						break;
					} else if (str[k] > 32) {
						break;
					}
				}

				if (col != Color()) {
					in_keyword = true;
					keyword_color = col;
				}
			}
		}

		if (!in_function_name && in_word && !in_keyword) {
			if (prev_text == GDScriptTokenizer::get_token_name(GDScriptTokenizer::Token::SIGNAL)) {
				in_signal_declaration = true;
			} else {
				int k = j;
				while (k < line_length && !is_symbol(str[k]) && !is_whitespace(str[k])) {
					k++;
				}

				// Check for space between name and bracket.
				while (k < line_length && is_whitespace(str[k])) {
					k++;
				}

				if (str[k] == '(') {
					in_function_name = true;
					if (prev_text == GDScriptTokenizer::get_token_name(GDScriptTokenizer::Token::FUNC)) {
						in_function_declaration = true;
					}
				} else if (prev_text == GDScriptTokenizer::get_token_name(GDScriptTokenizer::Token::VAR) || prev_text == GDScriptTokenizer::get_token_name(GDScriptTokenizer::Token::FOR) || prev_text == GDScriptTokenizer::get_token_name(GDScriptTokenizer::Token::CONST)) {
					in_var_const_declaration = true;
				}

				// Check for lambda.
				if (in_function_declaration) {
					k = j - 1;
					while (k > 0 && is_whitespace(str[k])) {
						k--;
					}

					if (str[k] == ':') {
						in_lambda = true;
					}
				}
			}
		}

		if (!in_function_name && !in_member_variable && !in_keyword && !in_number && in_word) {
			int k = j;
			while (k > 0 && !is_symbol(str[k]) && !is_whitespace(str[k])) {
				k--;
			}

			if (str[k] == '.') {
				in_member_variable = true;
			}
		}

		if (is_a_symbol) {
			if (in_function_declaration || in_signal_declaration) {
				is_after_func_signal_declaration = true;
			}
			if (in_var_const_declaration) {
				is_after_var_const_declaration = true;
			}

			if (in_declaration_params > 0) {
				switch (str[j]) {
					case '(':
						in_declaration_params += 1;
						break;
					case ')':
						in_declaration_params -= 1;
						break;
					case '{':
						in_declaration_param_dicts += 1;
						break;
					case '}':
						in_declaration_param_dicts -= 1;
						break;
				}
			} else if ((is_after_func_signal_declaration || prev_text == GDScriptTokenizer::get_token_name(GDScriptTokenizer::Token::FUNC)) && str[j] == '(') {
				in_declaration_params = 1;
				in_declaration_param_dicts = 0;
			}

			if (expect_type) {
				switch (str[j]) {
					case '[':
						in_type_params += 1;
						break;
					case ']':
						in_type_params -= 1;
						break;
					case ',':
						if (in_type_params <= 0) {
							expect_type = false;
						}
						break;
					case ' ':
					case '\t':
					case '.':
						break;
					default:
						expect_type = false;
						break;
				}
			} else {
				if (j > 0 && str[j - 1] == '-' && str[j] == '>') {
					expect_type = true;
					in_type_params = 0;
				}
				if ((is_after_var_const_declaration || (in_declaration_params == 1 && in_declaration_param_dicts == 0)) && str[j] == ':') {
					expect_type = true;
					in_type_params = 0;
				}
			}

			in_function_name = false;
			in_function_declaration = false;
			in_signal_declaration = false;
			in_var_const_declaration = false;
			in_lambda = false;
			in_member_variable = false;

			if (!is_whitespace(str[j])) {
				is_after_func_signal_declaration = false;
				is_after_var_const_declaration = false;
			}
		}

		// Set color of StringName, keeping symbol color for binary '&&' and '&'.
		if (!in_string_name && in_region == -1 && str[j] == '&' && !is_binary_op) {
			if (j + 1 <= line_length - 1 && (str[j + 1] == '\'' || str[j + 1] == '"')) {
				in_string_name = true;
				// Cover edge cases of i.e. '+&""' and '&&&""', so the StringName is properly colored.
				if (prev_is_binary_op && j >= 2 && str[j - 1] == '&' && str[j - 2] != '&') {
					in_string_name = false;
					is_binary_op = true;
				}
			} else {
				is_binary_op = true;
			}
		} else if (in_region != -1 || is_a_symbol) {
			in_string_name = false;
		}

		// '^^' has no special meaning, so unlike StringName, when binary, use NodePath color for the last caret.
		if (!in_node_path && in_region == -1 && str[j] == '^' && !is_binary_op && (j == 0 || (j > 0 && str[j - 1] != '^') || prev_is_binary_op)) {
			in_node_path = true;
		} else if (in_region != -1 || is_a_symbol) {
			in_node_path = false;
		}

		if (!in_node_ref && in_region == -1 && (str[j] == '$' || (str[j] == '%' && !is_binary_op))) {
			in_node_ref = true;
		} else if (in_region != -1 || (is_a_symbol && str[j] != '/' && str[j] != '%') || (is_a_digit && j > 0 && (str[j - 1] == '$' || str[j - 1] == '/' || str[j - 1] == '%'))) {
			// NodeRefs can't start with digits, so point out wrong syntax immediately.
			in_node_ref = false;
		}

		if (!in_annotation && in_region == -1 && str[j] == '@') {
			in_annotation = true;
		} else if (in_region != -1 || is_a_symbol) {
			in_annotation = false;
		}

		const bool in_raw_string_prefix = in_region == -1 && str[j] == 'r' && j + 1 < line_length && (str[j + 1] == '"' || str[j + 1] == '\'');

		if (in_raw_string_prefix) {
			color = string_color;
		} else if (in_node_ref) {
			next_type = NODE_REF;
			color = node_ref_color;
		} else if (in_annotation) {
			next_type = ANNOTATION;
			color = annotation_color;
		} else if (in_string_name) {
			next_type = STRING_NAME;
			color = string_name_color;
		} else if (in_node_path) {
			next_type = NODE_PATH;
			color = node_path_color;
		} else if (in_keyword) {
			next_type = KEYWORD;
			color = keyword_color;
		} else if (in_signal_declaration) {
			next_type = SIGNAL;
			color = member_color;
		} else if (in_function_name) {
			next_type = FUNCTION;
			if (!in_lambda && in_function_declaration) {
				color = function_definition_color;
			} else {
				color = function_color;
			}
		} else if (in_number) {
			next_type = NUMBER;
			color = number_color;
		} else if (is_a_symbol) {
			next_type = SYMBOL;
			color = symbol_color;
		} else if (expect_type) {
			next_type = TYPE;
			color = type_color;
		} else if (in_member_variable) {
			next_type = MEMBER;
			color = member_color;
		} else {
			next_type = IDENTIFIER;
		}

		if (next_type != current_type) {
			if (current_type == NONE) {
				current_type = next_type;
			} else {
				prev_type = current_type;
				current_type = next_type;

				// No need to store regions...
				if (prev_type == REGION) {
					prev_text = "";
					prev_column = j;
				} else {
					String text = str.substr(prev_column, j - prev_column).strip_edges();
					prev_column = j;

					// Ignore if just whitespace.
					if (!text.is_empty()) {
						prev_text = text;
					}
				}
			}
		}

		prev_is_char = is_char;
		prev_is_digit = is_a_digit;
		prev_is_binary_op = is_binary_op;

		if (color != prev_color) {
			prev_color = color;
			highlighter_info["color"] = color;
			color_map[j] = highlighter_info;
		}
	}
	return color_map;
}

String GDScriptSyntaxHighlighter::_get_name() const {
	return "GDScript";
}

PackedStringArray GDScriptSyntaxHighlighter::_get_supported_languages() const {
	PackedStringArray languages;
	languages.push_back("GDScript");
	return languages;
}

void GDScriptSyntaxHighlighter::_update_cache() {
	class_names.clear();
	reserved_keywords.clear();
	member_keywords.clear();
	global_functions.clear();
	color_regions.clear();
	color_region_cache.clear();

	font_color = text_edit->get_theme_color(SceneStringName(font_color));
	symbol_color = EDITOR_GET("text_editor/theme/highlighting/symbol_color");
	function_color = EDITOR_GET("text_editor/theme/highlighting/function_color");
	number_color = EDITOR_GET("text_editor/theme/highlighting/number_color");
	member_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");

	/* Engine types. */
	const Color types_color = EDITOR_GET("text_editor/theme/highlighting/engine_type_color");
	List<StringName> types;
	ClassDB::get_class_list(&types);
	for (const StringName &E : types) {
		if (ClassDB::is_class_exposed(E)) {
			class_names[E] = types_color;
		}
	}

	/* User types. */
	const Color usertype_color = EDITOR_GET("text_editor/theme/highlighting/user_type_color");
	List<StringName> global_classes;
	ScriptServer::get_global_class_list(&global_classes);
	for (const StringName &E : global_classes) {
		class_names[E] = usertype_color;
	}

	/* Autoloads. */
	for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : ProjectSettings::get_singleton()->get_autoload_list()) {
		const ProjectSettings::AutoloadInfo &info = E.value;
		if (info.is_singleton) {
			class_names[info.name] = usertype_color;
		}
	}

	const GDScriptLanguage *gdscript = GDScriptLanguage::get_singleton();

	/* Core types. */
	const Color basetype_color = EDITOR_GET("text_editor/theme/highlighting/base_type_color");
	List<String> core_types;
	gdscript->get_core_type_words(&core_types);
	for (const String &E : core_types) {
		class_names[StringName(E)] = basetype_color;
	}
	class_names[SNAME("Variant")] = basetype_color;
	class_names[SNAME("void")] = basetype_color;
	// `get_core_type_words()` doesn't return primitive types.
	class_names[SNAME("bool")] = basetype_color;
	class_names[SNAME("int")] = basetype_color;
	class_names[SNAME("float")] = basetype_color;

	/* Reserved words. */
	const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	const Color control_flow_keyword_color = EDITOR_GET("text_editor/theme/highlighting/control_flow_keyword_color");
	List<String> keyword_list;
	gdscript->get_reserved_words(&keyword_list);
	for (const String &E : keyword_list) {
		if (gdscript->is_control_flow_keyword(E)) {
			reserved_keywords[StringName(E)] = control_flow_keyword_color;
		} else {
			reserved_keywords[StringName(E)] = keyword_color;
		}
	}

	// Highlight `set` and `get` as "keywords" with the function color to avoid conflicts with method calls.
	reserved_keywords[SNAME("set")] = function_color;
	reserved_keywords[SNAME("get")] = function_color;

	/* Global functions. */
	List<StringName> global_function_list;
	GDScriptUtilityFunctions::get_function_list(&global_function_list);
	Variant::get_utility_function_list(&global_function_list);
	// "assert" and "preload" are not utility functions, but are global nonetheless, so insert them.
	global_functions.insert(SNAME("assert"));
	global_functions.insert(SNAME("preload"));
	for (const StringName &E : global_function_list) {
		global_functions.insert(E);
	}

	/* Comments */
	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	List<String> comments;
	gdscript->get_comment_delimiters(&comments);
	for (const String &comment : comments) {
		String beg = comment.get_slice(" ", 0);
		String end = comment.get_slice_count(" ") > 1 ? comment.get_slice(" ", 1) : String();
		add_color_region(ColorRegion::TYPE_COMMENT, beg, end, comment_color, end.is_empty());
	}

	/* Doc comments */
	const Color doc_comment_color = EDITOR_GET("text_editor/theme/highlighting/doc_comment_color");
	List<String> doc_comments;
	gdscript->get_doc_comment_delimiters(&doc_comments);
	for (const String &doc_comment : doc_comments) {
		String beg = doc_comment.get_slice(" ", 0);
		String end = doc_comment.get_slice_count(" ") > 1 ? doc_comment.get_slice(" ", 1) : String();
		add_color_region(ColorRegion::TYPE_COMMENT, beg, end, doc_comment_color, end.is_empty());
	}

	/* Code regions */
	const Color code_region_color = Color(EDITOR_GET("text_editor/theme/highlighting/folded_code_region_color").operator Color(), 1.0);
	add_color_region(ColorRegion::TYPE_CODE_REGION, "#region", "", code_region_color, true);
	add_color_region(ColorRegion::TYPE_CODE_REGION, "#endregion", "", code_region_color, true);

	/* Strings */
	string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	add_color_region(ColorRegion::TYPE_STRING, "\"", "\"", string_color);
	add_color_region(ColorRegion::TYPE_STRING, "'", "'", string_color);
	add_color_region(ColorRegion::TYPE_MULTILINE_STRING, "\"\"\"", "\"\"\"", string_color);
	add_color_region(ColorRegion::TYPE_MULTILINE_STRING, "'''", "'''", string_color);
	add_color_region(ColorRegion::TYPE_STRING, "\"", "\"", string_color, false, true);
	add_color_region(ColorRegion::TYPE_STRING, "'", "'", string_color, false, true);
	add_color_region(ColorRegion::TYPE_MULTILINE_STRING, "\"\"\"", "\"\"\"", string_color, false, true);
	add_color_region(ColorRegion::TYPE_MULTILINE_STRING, "'''", "'''", string_color, false, true);

	const Ref<Script> scr = _get_edited_resource();
	if (scr.is_valid()) {
		/* Member types. */
		const Color member_variable_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");
		StringName instance_base = scr->get_instance_base_type();
		if (instance_base != StringName()) {
			List<PropertyInfo> plist;
			ClassDB::get_property_list(instance_base, &plist);
			for (const PropertyInfo &E : plist) {
				String prop_name = E.name;
				if (E.usage & PROPERTY_USAGE_CATEGORY || E.usage & PROPERTY_USAGE_GROUP || E.usage & PROPERTY_USAGE_SUBGROUP) {
					continue;
				}
				if (prop_name.contains_char('/')) {
					continue;
				}
				member_keywords[prop_name] = member_variable_color;
			}

			List<String> clist;
			ClassDB::get_integer_constant_list(instance_base, &clist);
			for (const String &E : clist) {
				member_keywords[E] = member_variable_color;
			}
		}
	}

	const String text_edit_color_theme = EDITOR_GET("text_editor/theme/color_theme");
	const bool godot_2_theme = text_edit_color_theme == "Godot 2";

	if (godot_2_theme || EditorThemeManager::is_dark_theme()) {
		function_definition_color = Color(0.4, 0.9, 1.0);
		global_function_color = Color(0.64, 0.64, 0.96);
		node_path_color = Color(0.72, 0.77, 0.49);
		node_ref_color = Color(0.39, 0.76, 0.35);
		annotation_color = Color(1.0, 0.7, 0.45);
		string_name_color = Color(1.0, 0.76, 0.65);
		comment_marker_colors[COMMENT_MARKER_CRITICAL] = Color(0.77, 0.35, 0.35);
		comment_marker_colors[COMMENT_MARKER_WARNING] = Color(0.72, 0.61, 0.48);
		comment_marker_colors[COMMENT_MARKER_NOTICE] = Color(0.56, 0.67, 0.51);
	} else {
		function_definition_color = Color(0, 0.6, 0.6);
		global_function_color = Color(0.36, 0.18, 0.72);
		node_path_color = Color(0.18, 0.55, 0);
		node_ref_color = Color(0.0, 0.5, 0);
		annotation_color = Color(0.8, 0.37, 0);
		string_name_color = Color(0.8, 0.56, 0.45);
		comment_marker_colors[COMMENT_MARKER_CRITICAL] = Color(0.8, 0.14, 0.14);
		comment_marker_colors[COMMENT_MARKER_WARNING] = Color(0.75, 0.39, 0.03);
		comment_marker_colors[COMMENT_MARKER_NOTICE] = Color(0.24, 0.54, 0.09);
	}

	// TODO: Move to editor_settings.cpp
	EDITOR_DEF("text_editor/theme/highlighting/gdscript/function_definition_color", function_definition_color);
	EDITOR_DEF("text_editor/theme/highlighting/gdscript/global_function_color", global_function_color);
	EDITOR_DEF("text_editor/theme/highlighting/gdscript/node_path_color", node_path_color);
	EDITOR_DEF("text_editor/theme/highlighting/gdscript/node_reference_color", node_ref_color);
	EDITOR_DEF("text_editor/theme/highlighting/gdscript/annotation_color", annotation_color);
	EDITOR_DEF("text_editor/theme/highlighting/gdscript/string_name_color", string_name_color);
	EDITOR_DEF("text_editor/theme/highlighting/comment_markers/critical_color", comment_marker_colors[COMMENT_MARKER_CRITICAL]);
	EDITOR_DEF("text_editor/theme/highlighting/comment_markers/warning_color", comment_marker_colors[COMMENT_MARKER_WARNING]);
	EDITOR_DEF("text_editor/theme/highlighting/comment_markers/notice_color", comment_marker_colors[COMMENT_MARKER_NOTICE]);
	// The list is based on <https://github.com/KDE/syntax-highlighting/blob/master/data/syntax/alert.xml>.
	EDITOR_DEF("text_editor/theme/highlighting/comment_markers/critical_list", "ALERT,ATTENTION,CAUTION,CRITICAL,DANGER,SECURITY");
	EDITOR_DEF("text_editor/theme/highlighting/comment_markers/warning_list", "BUG,DEPRECATED,FIXME,HACK,TASK,TBD,TODO,WARNING");
	EDITOR_DEF("text_editor/theme/highlighting/comment_markers/notice_list", "INFO,NOTE,NOTICE,TEST,TESTING");

	if (text_edit_color_theme == "Default" || godot_2_theme) {
		EditorSettings::get_singleton()->set_initial_value(
				"text_editor/theme/highlighting/gdscript/function_definition_color",
				function_definition_color,
				true);
		EditorSettings::get_singleton()->set_initial_value(
				"text_editor/theme/highlighting/gdscript/global_function_color",
				global_function_color,
				true);
		EditorSettings::get_singleton()->set_initial_value(
				"text_editor/theme/highlighting/gdscript/node_path_color",
				node_path_color,
				true);
		EditorSettings::get_singleton()->set_initial_value(
				"text_editor/theme/highlighting/gdscript/node_reference_color",
				node_ref_color,
				true);
		EditorSettings::get_singleton()->set_initial_value(
				"text_editor/theme/highlighting/gdscript/annotation_color",
				annotation_color,
				true);
		EditorSettings::get_singleton()->set_initial_value(
				"text_editor/theme/highlighting/gdscript/string_name_color",
				string_name_color,
				true);
		EditorSettings::get_singleton()->set_initial_value(
				"text_editor/theme/highlighting/comment_markers/critical_color",
				comment_marker_colors[COMMENT_MARKER_CRITICAL],
				true);
		EditorSettings::get_singleton()->set_initial_value(
				"text_editor/theme/highlighting/comment_markers/warning_color",
				comment_marker_colors[COMMENT_MARKER_WARNING],
				true);
		EditorSettings::get_singleton()->set_initial_value(
				"text_editor/theme/highlighting/comment_markers/notice_color",
				comment_marker_colors[COMMENT_MARKER_NOTICE],
				true);
	}

	function_definition_color = EDITOR_GET("text_editor/theme/highlighting/gdscript/function_definition_color");
	global_function_color = EDITOR_GET("text_editor/theme/highlighting/gdscript/global_function_color");
	node_path_color = EDITOR_GET("text_editor/theme/highlighting/gdscript/node_path_color");
	node_ref_color = EDITOR_GET("text_editor/theme/highlighting/gdscript/node_reference_color");
	annotation_color = EDITOR_GET("text_editor/theme/highlighting/gdscript/annotation_color");
	string_name_color = EDITOR_GET("text_editor/theme/highlighting/gdscript/string_name_color");
	type_color = EDITOR_GET("text_editor/theme/highlighting/base_type_color");
	comment_marker_colors[COMMENT_MARKER_CRITICAL] = EDITOR_GET("text_editor/theme/highlighting/comment_markers/critical_color");
	comment_marker_colors[COMMENT_MARKER_WARNING] = EDITOR_GET("text_editor/theme/highlighting/comment_markers/warning_color");
	comment_marker_colors[COMMENT_MARKER_NOTICE] = EDITOR_GET("text_editor/theme/highlighting/comment_markers/notice_color");

	comment_markers.clear();
	Vector<String> critical_list = EDITOR_GET("text_editor/theme/highlighting/comment_markers/critical_list").operator String().split(",", false);
	for (int i = 0; i < critical_list.size(); i++) {
		comment_markers[critical_list[i]] = COMMENT_MARKER_CRITICAL;
	}
	Vector<String> warning_list = EDITOR_GET("text_editor/theme/highlighting/comment_markers/warning_list").operator String().split(",", false);
	for (int i = 0; i < warning_list.size(); i++) {
		comment_markers[warning_list[i]] = COMMENT_MARKER_WARNING;
	}
	Vector<String> notice_list = EDITOR_GET("text_editor/theme/highlighting/comment_markers/notice_list").operator String().split(",", false);
	for (int i = 0; i < notice_list.size(); i++) {
		comment_markers[notice_list[i]] = COMMENT_MARKER_NOTICE;
	}
}

void GDScriptSyntaxHighlighter::add_color_region(ColorRegion::Type p_type, const String &p_start_key, const String &p_end_key, const Color &p_color, bool p_line_only, bool p_r_prefix) {
	ERR_FAIL_COND_MSG(p_start_key.is_empty(), "Color region start key cannot be empty.");
	ERR_FAIL_COND_MSG(!is_symbol(p_start_key[0]), "Color region start key must start with a symbol.");

	if (!p_end_key.is_empty()) {
		ERR_FAIL_COND_MSG(!is_symbol(p_end_key[0]), "Color region end key must start with a symbol.");
	}

	int at = 0;
	for (const ColorRegion &region : color_regions) {
		ERR_FAIL_COND_MSG(region.start_key == p_start_key && region.r_prefix == p_r_prefix, "Color region with start key '" + p_start_key + "' already exists.");
		if (p_start_key.length() < region.start_key.length()) {
			at++;
		} else {
			break;
		}
	}

	ColorRegion color_region;
	color_region.type = p_type;
	color_region.color = p_color;
	color_region.start_key = p_start_key;
	color_region.end_key = p_end_key;
	color_region.line_only = p_line_only;
	color_region.r_prefix = p_r_prefix;
	color_region.is_string = p_type == ColorRegion::TYPE_STRING || p_type == ColorRegion::TYPE_MULTILINE_STRING;
	color_region.is_comment = p_type == ColorRegion::TYPE_COMMENT || p_type == ColorRegion::TYPE_CODE_REGION;
	color_regions.insert(at, color_region);
	clear_highlighting_cache();
}

Ref<EditorSyntaxHighlighter> GDScriptSyntaxHighlighter::_create() const {
	Ref<GDScriptSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}
