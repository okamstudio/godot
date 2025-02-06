/**************************************************************************/
/*  char_utils.h                                                          */
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

#ifndef CHAR_UTILS_H
#define CHAR_UTILS_H

#include "core/typedefs.h"

#include "char_range.inc"

#define BSEARCH_CHAR_RANGE(m_array)                      \
	int low = 0;                                         \
	int high = sizeof(m_array) / sizeof(m_array[0]) - 1; \
	int middle = (low + high) / 2;                       \
                                                         \
	while (low <= high) {                                \
		if (p_char < m_array[middle].start) {            \
			high = middle - 1;                           \
		} else if (p_char > m_array[middle].end) {       \
			low = middle + 1;                            \
		} else {                                         \
			return true;                                 \
		}                                                \
                                                         \
		middle = (low + high) / 2;                       \
	}                                                    \
                                                         \
	return false

constexpr bool is_unicode_identifier_start(char32_t p_char) {
	BSEARCH_CHAR_RANGE(xid_start);
}

constexpr bool is_unicode_identifier_continue(char32_t p_char) {
	BSEARCH_CHAR_RANGE(xid_continue);
}

constexpr bool is_unicode_upper_case(char32_t p_char) {
	BSEARCH_CHAR_RANGE(uppercase_letter);
}

constexpr bool is_unicode_lower_case(char32_t p_char) {
	BSEARCH_CHAR_RANGE(lowercase_letter);
}

constexpr bool is_unicode_letter(char32_t p_char) {
	BSEARCH_CHAR_RANGE(unicode_letter);
}

#undef BSEARCH_CHAR_RANGE

constexpr bool is_ascii_upper_case(char32_t p_char) {
	return (p_char >= 'A' && p_char <= 'Z');
}

constexpr bool is_ascii_lower_case(char32_t p_char) {
	return (p_char >= 'a' && p_char <= 'z');
}

constexpr bool is_digit(char32_t p_char) {
	return (p_char >= '0' && p_char <= '9');
}

constexpr bool is_hex_digit(char32_t p_char) {
	return (is_digit(p_char) || (p_char >= 'a' && p_char <= 'f') || (p_char >= 'A' && p_char <= 'F'));
}

constexpr bool is_binary_digit(char32_t p_char) {
	return (p_char == '0' || p_char == '1');
}

constexpr bool is_ascii_alphabet_char(char32_t p_char) {
	return (p_char >= 'a' && p_char <= 'z') || (p_char >= 'A' && p_char <= 'Z');
}

constexpr bool is_ascii_alphanumeric_char(char32_t p_char) {
	return (p_char >= 'a' && p_char <= 'z') || (p_char >= 'A' && p_char <= 'Z') || (p_char >= '0' && p_char <= '9');
}

constexpr bool is_ascii_identifier_char(char32_t p_char) {
	return (p_char >= 'a' && p_char <= 'z') || (p_char >= 'A' && p_char <= 'Z') || (p_char >= '0' && p_char <= '9') || p_char == '_';
}

constexpr bool is_symbol(char32_t p_char) {
	return p_char != '_' && ((p_char >= '!' && p_char <= '/') || (p_char >= ':' && p_char <= '@') || (p_char >= '[' && p_char <= '`') || (p_char >= '{' && p_char <= '~') || p_char == '\t' || p_char == ' ');
}

constexpr bool is_control(char32_t p_char) {
	return (p_char <= 0x001f) || (p_char >= 0x007f && p_char <= 0x009f);
}

constexpr bool is_whitespace(char32_t p_char) {
	return (p_char == ' ') || (p_char == 0x00a0) || (p_char == 0x1680) || (p_char >= 0x2000 && p_char <= 0x200b) || (p_char == 0x202f) || (p_char == 0x205f) || (p_char == 0x3000) || (p_char == 0x2028) || (p_char == 0x2029) || (p_char >= 0x0009 && p_char <= 0x000d) || (p_char == 0x0085);
}

constexpr bool is_linebreak(char32_t p_char) {
	return (p_char >= 0x000a && p_char <= 0x000d) || (p_char == 0x0085) || (p_char == 0x2028) || (p_char == 0x2029);
}

constexpr bool is_punct(char32_t p_char) {
	return (p_char >= ' ' && p_char <= '/') || (p_char >= ':' && p_char <= '@') || (p_char >= '[' && p_char <= '^') || (p_char == '`') || (p_char >= '{' && p_char <= '~') || (p_char >= 0x2000 && p_char <= 0x206f) || (p_char >= 0x3000 && p_char <= 0x303f);
}

constexpr bool is_underscore(char32_t p_char) {
	return (p_char == '_');
}

#endif // CHAR_UTILS_H
