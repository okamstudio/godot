/**************************************************************************/
/*  syntax_highlighter.cpp                                                */
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

#include "syntax_highlighter.h"

#include "core/object/script_language.h"
#include "scene/gui/text_edit.h"

Dictionary SyntaxHighlighter::get_line_syntax_highlighting(int p_line) {
	if (highlighting_cache.has(p_line)) {
		return highlighting_cache[p_line];
	}

	Dictionary color_map;
	if (text_edit == nullptr) {
		return color_map;
	}

	if (!GDVIRTUAL_CALL(_get_line_syntax_highlighting, p_line, color_map)) {
		color_map = _get_line_syntax_highlighting_impl(p_line);
	}

	highlighting_cache[p_line] = color_map;
	return color_map;
}

void SyntaxHighlighter::_lines_edited_from(int p_from_line, int p_to_line) {
	if (highlighting_cache.size() < 1) {
		return;
	}

	int cache_size = highlighting_cache.back()->key();
	for (int i = MIN(p_from_line, p_to_line) - 1; i <= cache_size; i++) {
		if (highlighting_cache.has(i)) {
			highlighting_cache.erase(i);
		}
	}
}

void SyntaxHighlighter::clear_highlighting_cache() {
	highlighting_cache.clear();

	if (GDVIRTUAL_CALL(_clear_highlighting_cache)) {
		return;
	}
	_clear_highlighting_cache();
}

void SyntaxHighlighter::update_cache() {
	clear_highlighting_cache();

	if (text_edit == nullptr) {
		return;
	}
	if (GDVIRTUAL_CALL(_update_cache)) {
		return;
	}
	_update_cache();
}

void SyntaxHighlighter::set_text_edit(TextEdit *p_text_edit) {
	if (text_edit && ObjectDB::get_instance(text_edit_instance_id)) {
		text_edit->disconnect("lines_edited_from", callable_mp(this, &SyntaxHighlighter::_lines_edited_from));
	}

	text_edit = p_text_edit;
	if (p_text_edit == nullptr) {
		return;
	}
	text_edit_instance_id = text_edit->get_instance_id();
	text_edit->connect("lines_edited_from", callable_mp(this, &SyntaxHighlighter::_lines_edited_from));
	update_cache();
}

TextEdit *SyntaxHighlighter::get_text_edit() const {
	return text_edit;
}

void SyntaxHighlighter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_line_syntax_highlighting", "line"), &SyntaxHighlighter::get_line_syntax_highlighting);
	ClassDB::bind_method(D_METHOD("update_cache"), &SyntaxHighlighter::update_cache);
	ClassDB::bind_method(D_METHOD("clear_highlighting_cache"), &SyntaxHighlighter::clear_highlighting_cache);
	ClassDB::bind_method(D_METHOD("get_text_edit"), &SyntaxHighlighter::get_text_edit);

	GDVIRTUAL_BIND(_get_line_syntax_highlighting, "line")
	GDVIRTUAL_BIND(_clear_highlighting_cache)
	GDVIRTUAL_BIND(_update_cache)
}

////////////////////////////////////////////////////////////////////////////////

Dictionary CodeHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	Dictionary color_map;

	bool prev_is_char = false;
	bool prev_is_number = false;
	bool in_keyword = false;
	bool in_word = false;
	bool in_function_name = false;
	bool in_member_variable = false;
	bool is_hex_notation = false;
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

	if (in_region != -1 && str.length() == 0) {
		color_region_cache[p_line] = in_region;
	}
	for (int j = 0; j < line_length; j++) {
		Dictionary highlighter_info;

		color = font_color;
		bool is_char = !is_symbol(str[j]);
		bool is_a_symbol = is_symbol(str[j]);
		bool is_number = is_digit(str[j]);

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
				/* check if we are in entering a region */
				if (in_region == -1) {
					for (int c = 0; c < color_regions.size(); c++) {
						/* check there is enough room */
						int chars_left = line_length - from;
						int start_key_length = color_regions[c].start_key.length();
						int end_key_length = color_regions[c].end_key.length();
						if (chars_left < start_key_length) {
							continue;
						}

						/* search the line */
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

						/* check if it's the whole line */
						if (end_key_length == 0 || color_regions[c].line_only || from + end_key_length > line_length) {
							if (from + end_key_length > line_length && (color_regions[in_region].start_key == "\"" || color_regions[in_region].start_key == "\'")) {
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

					if (j == line_length) {
						continue;
					}
				}

				/* if we are in one find the end key */
				if (in_region != -1) {
					bool is_string = (color_regions[in_region].start_key == "\"" || color_regions[in_region].start_key == "\'");

					Color region_color = color_regions[in_region].color;
					prev_color = region_color;
					highlighter_info["color"] = region_color;
					color_map[j] = highlighter_info;

					/* search the line */
					int region_end_index = -1;
					int end_key_length = color_regions[in_region].end_key.length();
					const char32_t *end_key = color_regions[in_region].end_key.get_data();
					for (; from < line_length; from++) {
						if (line_length - from < end_key_length) {
							// Don't break if '\' to highlight esc chars.
							if (!is_string || str.find_char('\\', from) < 0) {
								break;
							}
						}

						if (!is_symbol(str[from])) {
							continue;
						}

						if (str[from] == '\\') {
							if (is_string) {
								Dictionary escape_char_highlighter_info;
								escape_char_highlighter_info["color"] = symbol_color;
								color_map[from] = escape_char_highlighter_info;
							}

							from++;

							if (is_string) {
								Dictionary region_continue_highlighter_info;
								prev_color = region_color;
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

					in_region = -1;
					prev_is_char = false;
					prev_is_number = false;
					continue;
				}
			}
		}

		// Allow ABCDEF in hex notation.
		if (is_hex_notation && (is_hex_digit(str[j]) || is_number)) {
			is_number = true;
		} else {
			is_hex_notation = false;
		}

		// Check for dot or underscore or 'x' for hex notation in floating point number or 'e' for scientific notation.
		if ((str[j] == '.' || str[j] == 'x' || str[j] == '_' || str[j] == 'f' || str[j] == 'e' || (uint_suffix_enabled && str[j] == 'u')) && !in_word && prev_is_number && !is_number) {
			is_number = true;
			is_a_symbol = false;
			is_char = false;

			if (str[j] == 'x' && str[j - 1] == '0') {
				is_hex_notation = true;
			}
		}

		if (!in_word && (is_ascii_alphabet_char(str[j]) || is_underscore(str[j])) && !is_number) {
			in_word = true;
		}

		if ((in_keyword || in_word) && !is_hex_notation) {
			is_number = false;
		}

		if (is_a_symbol && str[j] != '.' && in_word) {
			in_word = false;
		}

		if (!is_char) {
			in_keyword = false;
		}

		if (!in_keyword && is_char && !prev_is_char) {
			int to = j;
			while (to < line_length && !is_symbol(str[to])) {
				to++;
			}

			String word = str.substr(j, to - j);
			Color col;
			if (keywords.has(word)) {
				col = keywords[word];
			} else if (member_keywords.has(word)) {
				col = member_keywords[word];
				for (int k = j - 1; k >= 0; k--) {
					if (str[k] == '.') {
						col = Color(); //member indexing not allowed
						break;
					} else if (str[k] > 32) {
						break;
					}
				}
			}

			if (col != Color()) {
				in_keyword = true;
				keyword_color = col;
			}
		}

		if (!in_function_name && in_word && !in_keyword) {
			int k = j;
			while (k < line_length && !is_symbol(str[k]) && str[k] != '\t' && str[k] != ' ') {
				k++;
			}

			// Check for space between name and bracket.
			while (k < line_length && (str[k] == '\t' || str[k] == ' ')) {
				k++;
			}

			if (str[k] == '(') {
				in_function_name = true;
			}
		}

		if (!in_function_name && !in_member_variable && !in_keyword && !is_number && in_word) {
			int k = j;
			while (k > 0 && !is_symbol(str[k]) && str[k] != '\t' && str[k] != ' ') {
				k--;
			}

			if (str[k] == '.') {
				in_member_variable = true;
			}
		}

		if (is_a_symbol) {
			in_function_name = false;
			in_member_variable = false;
		}

		if (in_keyword) {
			color = keyword_color;
		} else if (in_member_variable) {
			color = member_color;
		} else if (in_function_name) {
			color = function_color;
		} else if (is_a_symbol) {
			color = symbol_color;
		} else if (is_number) {
			color = number_color;
		}

		prev_is_char = is_char;
		prev_is_number = is_number;

		if (color != prev_color) {
			prev_color = color;
			highlighter_info["color"] = color;
			color_map[j] = highlighter_info;
		}
	}

	return color_map;
}

void CodeHighlighter::_clear_highlighting_cache() {
	color_region_cache.clear();
}

void CodeHighlighter::_update_cache() {
	font_color = text_edit->get_font_color();
}

void CodeHighlighter::add_keyword_color(const String &p_keyword, const Color &p_color) {
	keywords[p_keyword] = p_color;
	clear_highlighting_cache();
}

void CodeHighlighter::remove_keyword_color(const String &p_keyword) {
	keywords.erase(p_keyword);
	clear_highlighting_cache();
}

bool CodeHighlighter::has_keyword_color(const String &p_keyword) const {
	return keywords.has(p_keyword);
}

Color CodeHighlighter::get_keyword_color(const String &p_keyword) const {
	ERR_FAIL_COND_V(!keywords.has(p_keyword), Color());
	return keywords[p_keyword];
}

void CodeHighlighter::set_keyword_colors(const Dictionary p_keywords) {
	keywords = p_keywords;
	clear_highlighting_cache();
}

void CodeHighlighter::clear_keyword_colors() {
	keywords.clear();
	clear_highlighting_cache();
}

Dictionary CodeHighlighter::get_keyword_colors() const {
	return keywords;
}

void CodeHighlighter::add_member_keyword_color(const String &p_member_keyword, const Color &p_color) {
	member_keywords[p_member_keyword] = p_color;
	clear_highlighting_cache();
}

void CodeHighlighter::remove_member_keyword_color(const String &p_member_keyword) {
	member_keywords.erase(p_member_keyword);
	clear_highlighting_cache();
}

bool CodeHighlighter::has_member_keyword_color(const String &p_member_keyword) const {
	return member_keywords.has(p_member_keyword);
}

Color CodeHighlighter::get_member_keyword_color(const String &p_member_keyword) const {
	ERR_FAIL_COND_V(!member_keywords.has(p_member_keyword), Color());
	return member_keywords[p_member_keyword];
}

void CodeHighlighter::set_member_keyword_colors(const Dictionary &p_member_keywords) {
	member_keywords = p_member_keywords;
	clear_highlighting_cache();
}

void CodeHighlighter::clear_member_keyword_colors() {
	member_keywords.clear();
	clear_highlighting_cache();
}

Dictionary CodeHighlighter::get_member_keyword_colors() const {
	return member_keywords;
}

void CodeHighlighter::add_color_region(const String &p_start_key, const String &p_end_key, const Color &p_color, bool p_line_only) {
	for (int i = 0; i < p_start_key.length(); i++) {
		ERR_FAIL_COND_MSG(!is_symbol(p_start_key[i]), "color regions must start with a symbol");
	}

	if (p_end_key.length() > 0) {
		for (int i = 0; i < p_end_key.length(); i++) {
			ERR_FAIL_COND_MSG(!is_symbol(p_end_key[i]), "color regions must end with a symbol");
		}
	}

	int at = 0;
	for (int i = 0; i < color_regions.size(); i++) {
		ERR_FAIL_COND_MSG(color_regions[i].start_key == p_start_key, "color region with start key '" + p_start_key + "' already exists.");
		if (p_start_key.length() < color_regions[i].start_key.length()) {
			at++;
		}
	}

	ColorRegion color_region;
	color_region.color = p_color;
	color_region.start_key = p_start_key;
	color_region.end_key = p_end_key;
	color_region.line_only = p_line_only || p_end_key.is_empty();
	color_regions.insert(at, color_region);
	clear_highlighting_cache();
}

void CodeHighlighter::remove_color_region(const String &p_start_key) {
	for (int i = 0; i < color_regions.size(); i++) {
		if (color_regions[i].start_key == p_start_key) {
			color_regions.remove_at(i);
			break;
		}
	}
	clear_highlighting_cache();
}

bool CodeHighlighter::has_color_region(const String &p_start_key) const {
	for (int i = 0; i < color_regions.size(); i++) {
		if (color_regions[i].start_key == p_start_key) {
			return true;
		}
	}
	return false;
}

void CodeHighlighter::set_color_regions(const Dictionary &p_color_regions) {
	color_regions.clear();

	List<Variant> keys;
	p_color_regions.get_key_list(&keys);

	for (const Variant &E : keys) {
		String key = E;

		String start_key = key.get_slice(" ", 0);
		String end_key = key.get_slice_count(" ") > 1 ? key.get_slice(" ", 1) : String();

		add_color_region(start_key, end_key, p_color_regions[key], end_key.is_empty());
	}
	clear_highlighting_cache();
}

void CodeHighlighter::clear_color_regions() {
	color_regions.clear();
	clear_highlighting_cache();
}

Dictionary CodeHighlighter::get_color_regions() const {
	Dictionary r_color_regions;
	for (int i = 0; i < color_regions.size(); i++) {
		ColorRegion region = color_regions[i];
		r_color_regions[region.start_key + (region.end_key.is_empty() ? "" : " " + region.end_key)] = region.color;
	}
	return r_color_regions;
}

void CodeHighlighter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_keyword_color", "keyword", "color"), &CodeHighlighter::add_keyword_color);
	ClassDB::bind_method(D_METHOD("remove_keyword_color", "keyword"), &CodeHighlighter::remove_keyword_color);
	ClassDB::bind_method(D_METHOD("has_keyword_color", "keyword"), &CodeHighlighter::has_keyword_color);
	ClassDB::bind_method(D_METHOD("get_keyword_color", "keyword"), &CodeHighlighter::get_keyword_color);

	ClassDB::bind_method(D_METHOD("set_keyword_colors", "keywords"), &CodeHighlighter::set_keyword_colors);
	ClassDB::bind_method(D_METHOD("clear_keyword_colors"), &CodeHighlighter::clear_keyword_colors);
	ClassDB::bind_method(D_METHOD("get_keyword_colors"), &CodeHighlighter::get_keyword_colors);

	ClassDB::bind_method(D_METHOD("add_member_keyword_color", "member_keyword", "color"), &CodeHighlighter::add_member_keyword_color);
	ClassDB::bind_method(D_METHOD("remove_member_keyword_color", "member_keyword"), &CodeHighlighter::remove_member_keyword_color);
	ClassDB::bind_method(D_METHOD("has_member_keyword_color", "member_keyword"), &CodeHighlighter::has_member_keyword_color);
	ClassDB::bind_method(D_METHOD("get_member_keyword_color", "member_keyword"), &CodeHighlighter::get_member_keyword_color);

	ClassDB::bind_method(D_METHOD("set_member_keyword_colors", "member_keyword"), &CodeHighlighter::set_member_keyword_colors);
	ClassDB::bind_method(D_METHOD("clear_member_keyword_colors"), &CodeHighlighter::clear_member_keyword_colors);
	ClassDB::bind_method(D_METHOD("get_member_keyword_colors"), &CodeHighlighter::get_member_keyword_colors);

	ClassDB::bind_method(D_METHOD("add_color_region", "start_key", "end_key", "color", "line_only"), &CodeHighlighter::add_color_region, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_color_region", "start_key"), &CodeHighlighter::remove_color_region);
	ClassDB::bind_method(D_METHOD("has_color_region", "start_key"), &CodeHighlighter::has_color_region);

	ClassDB::bind_method(D_METHOD("set_color_regions", "color_regions"), &CodeHighlighter::set_color_regions);
	ClassDB::bind_method(D_METHOD("clear_color_regions"), &CodeHighlighter::clear_color_regions);
	ClassDB::bind_method(D_METHOD("get_color_regions"), &CodeHighlighter::get_color_regions);

	ClassDB::bind_method(D_METHOD("set_function_color", "color"), &CodeHighlighter::set_function_color);
	ClassDB::bind_method(D_METHOD("get_function_color"), &CodeHighlighter::get_function_color);

	ClassDB::bind_method(D_METHOD("set_number_color", "color"), &CodeHighlighter::set_number_color);
	ClassDB::bind_method(D_METHOD("get_number_color"), &CodeHighlighter::get_number_color);

	ClassDB::bind_method(D_METHOD("set_symbol_color", "color"), &CodeHighlighter::set_symbol_color);
	ClassDB::bind_method(D_METHOD("get_symbol_color"), &CodeHighlighter::get_symbol_color);

	ClassDB::bind_method(D_METHOD("set_member_variable_color", "color"), &CodeHighlighter::set_member_variable_color);
	ClassDB::bind_method(D_METHOD("get_member_variable_color"), &CodeHighlighter::get_member_variable_color);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "number_color"), "set_number_color", "get_number_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "symbol_color"), "set_symbol_color", "get_symbol_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "function_color"), "set_function_color", "get_function_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "member_variable_color"), "set_member_variable_color", "get_member_variable_color");

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "keyword_colors"), "set_keyword_colors", "get_keyword_colors");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "member_keyword_colors"), "set_member_keyword_colors", "get_member_keyword_colors");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "color_regions"), "set_color_regions", "get_color_regions");
}

void CodeHighlighter::set_uint_suffix_enabled(bool p_enabled) {
	uint_suffix_enabled = p_enabled;
}

void CodeHighlighter::set_number_color(Color p_color) {
	number_color = p_color;
	clear_highlighting_cache();
}

Color CodeHighlighter::get_number_color() const {
	return number_color;
}

void CodeHighlighter::set_symbol_color(Color p_color) {
	symbol_color = p_color;
	clear_highlighting_cache();
}

Color CodeHighlighter::get_symbol_color() const {
	return symbol_color;
}

void CodeHighlighter::set_function_color(Color p_color) {
	function_color = p_color;
	clear_highlighting_cache();
}

Color CodeHighlighter::get_function_color() const {
	return function_color;
}

void CodeHighlighter::set_member_variable_color(Color p_color) {
	member_color = p_color;
	clear_highlighting_cache();
}

Color CodeHighlighter::get_member_variable_color() const {
	return member_color;
}
