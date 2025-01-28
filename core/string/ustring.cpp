/**************************************************************************/
/*  ustring.cpp                                                           */
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

#include "ustring.h"

#include "core/crypto/crypto_core.h"
#include "core/math/color.h"
#include "core/math/math_funcs.h"
#include "core/object/object.h"
#include "core/string/print_string.h"
#include "core/string/string_name.h"
#include "core/string/translation_server.h"
#include "core/string/ucaps.h"
#include "core/variant/variant.h"
#include "core/version_generated.gen.h"

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS // to disable build-time warning which suggested to use strcpy_s instead strcpy
#endif

#if defined(MINGW_ENABLED) || defined(_MSC_VER)
#define snprintf _snprintf_s
#endif

static const int MAX_DECIMALS = 32;

static _FORCE_INLINE_ char32_t lower_case(char32_t c) {
	return (is_ascii_upper_case(c) ? (c + ('a' - 'A')) : c);
}

const char CharString::_null = 0;
const char16_t Char16String::_null = 0;
const char32_t String::_null = 0;
const char32_t String::_replacement_char = 0xfffd;

bool select_word(const String &p_s, int p_col, int &r_beg, int &r_end) {
	const String &s = p_s;
	int beg = CLAMP(p_col, 0, s.length());
	int end = beg;

	if (s[beg] > 32 || beg == s.length()) {
		bool symbol = beg < s.length() && is_symbol(s[beg]);

		while (beg > 0 && s[beg - 1] > 32 && (symbol == is_symbol(s[beg - 1]))) {
			beg--;
		}
		while (end < s.length() && s[end + 1] > 32 && (symbol == is_symbol(s[end + 1]))) {
			end++;
		}

		if (end < s.length()) {
			end += 1;
		}

		r_beg = beg;
		r_end = end;

		return true;
	} else {
		return false;
	}
}

/*************************************************************************/
/*  Char16String                                                         */
/*************************************************************************/

bool Char16String::operator<(const Char16String &p_right) const {
	if (length() == 0) {
		return p_right.length() != 0;
	}

	return is_str_less(get_data(), p_right.get_data());
}

Char16String &Char16String::operator+=(char16_t p_char) {
	const int lhs_len = length();
	resize(lhs_len + 2);

	char16_t *dst = ptrw();
	dst[lhs_len] = p_char;
	dst[lhs_len + 1] = 0;

	return *this;
}

void Char16String::operator=(const char16_t *p_cstr) {
	copy_from(p_cstr);
}

const char16_t *Char16String::get_data() const {
	if (size()) {
		return &operator[](0);
	} else {
		return u"";
	}
}

void Char16String::copy_from(const char16_t *p_cstr) {
	if (!p_cstr) {
		resize(0);
		return;
	}

	const char16_t *s = p_cstr;
	for (; *s; s++) {
	}
	size_t len = s - p_cstr;

	if (len == 0) {
		resize(0);
		return;
	}

	Error err = resize(++len); // include terminating null char

	ERR_FAIL_COND_MSG(err != OK, "Failed to copy char16_t string.");

	memcpy(ptrw(), p_cstr, len * sizeof(char16_t));
}

/*************************************************************************/
/*  CharString                                                           */
/*************************************************************************/

bool CharString::operator<(const CharString &p_right) const {
	if (length() == 0) {
		return p_right.length() != 0;
	}

	return is_str_less(get_data(), p_right.get_data());
}

bool CharString::operator==(const CharString &p_right) const {
	if (length() == 0) {
		// True if both have length 0, false if only p_right has a length
		return p_right.length() == 0;
	} else if (p_right.length() == 0) {
		// False due to unequal length
		return false;
	}

	return strcmp(ptr(), p_right.ptr()) == 0;
}

CharString &CharString::operator+=(char p_char) {
	const int lhs_len = length();
	resize(lhs_len + 2);

	char *dst = ptrw();
	dst[lhs_len] = p_char;
	dst[lhs_len + 1] = 0;

	return *this;
}

void CharString::operator=(const char *p_cstr) {
	copy_from(p_cstr);
}

const char *CharString::get_data() const {
	if (size()) {
		return &operator[](0);
	} else {
		return "";
	}
}

void CharString::copy_from(const char *p_cstr) {
	if (!p_cstr) {
		resize(0);
		return;
	}

	size_t len = strlen(p_cstr);

	if (len == 0) {
		resize(0);
		return;
	}

	Error err = resize(++len); // include terminating null char

	ERR_FAIL_COND_MSG(err != OK, "Failed to copy C-string.");

	memcpy(ptrw(), p_cstr, len);
}

/*************************************************************************/
/*  String                                                               */
/*************************************************************************/

Error String::parse_url(String &r_scheme, String &r_host, int &r_port, String &r_path, String &r_fragment) const {
	// Splits the URL into scheme, host, port, path, fragment. Strip credentials when present.
	String base = *this;
	r_scheme = "";
	r_host = "";
	r_port = 0;
	r_path = "";
	r_fragment = "";

	int pos = base.find("://");
	// Scheme
	if (pos != -1) {
		bool is_scheme_valid = true;
		for (int i = 0; i < pos; i++) {
			if (!is_ascii_alphanumeric_char(base[i]) && base[i] != '+' && base[i] != '-' && base[i] != '.') {
				is_scheme_valid = false;
				break;
			}
		}
		if (is_scheme_valid) {
			r_scheme = base.substr(0, pos + 3).to_lower();
			base = base.substr(pos + 3, base.length() - pos - 3);
		}
	}
	pos = base.find_char('#');
	// Fragment
	if (pos != -1) {
		r_fragment = base.substr(pos + 1);
		base = base.substr(0, pos);
	}
	pos = base.find_char('/');
	// Path
	if (pos != -1) {
		r_path = base.substr(pos, base.length() - pos);
		base = base.substr(0, pos);
	}
	// Host
	pos = base.find_char('@');
	if (pos != -1) {
		// Strip credentials
		base = base.substr(pos + 1, base.length() - pos - 1);
	}
	if (base.begins_with("[")) {
		// Literal IPv6
		pos = base.rfind_char(']');
		if (pos == -1) {
			return ERR_INVALID_PARAMETER;
		}
		r_host = base.substr(1, pos - 1);
		base = base.substr(pos + 1, base.length() - pos - 1);
	} else {
		// Anything else
		if (base.get_slice_count(":") > 2) {
			return ERR_INVALID_PARAMETER;
		}
		pos = base.rfind_char(':');
		if (pos == -1) {
			r_host = base;
			base = "";
		} else {
			r_host = base.substr(0, pos);
			base = base.substr(pos, base.length() - pos);
		}
	}
	if (r_host.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	r_host = r_host.to_lower();
	// Port
	if (base.begins_with(":")) {
		base = base.substr(1, base.length() - 1);
		if (!base.is_valid_int()) {
			return ERR_INVALID_PARAMETER;
		}
		r_port = base.to_int();
		if (r_port < 1 || r_port > 65535) {
			return ERR_INVALID_PARAMETER;
		}
	}
	return OK;
}

void String::parse_latin1(const StrRange<char> &p_cstr) {
	if (p_cstr.len == 0) {
		resize(0);
		return;
	}

	resize(p_cstr.len + 1); // include 0

	const char *src = p_cstr.c_str;
	const char *end = src + p_cstr.len;
	char32_t *dst = ptrw();

	for (; src < end; ++src, ++dst) {
		// If char is int8_t, a set sign bit will be reinterpreted as 256 - val implicitly.
		*dst = static_cast<uint8_t>(*src);
	}
	*dst = 0;
}

void String::parse_utf32(const StrRange<char32_t> &p_cstr) {
	if (p_cstr.len == 0) {
		resize(0);
		return;
	}

	copy_from_unchecked(p_cstr.c_str, p_cstr.len);
}

void String::parse_utf32(const char32_t &p_char) {
	if (p_char == 0) {
		print_unicode_error("NUL character", true);
		return;
	}

	resize(2);

	char32_t *dst = ptrw();

	if ((p_char & 0xfffff800) == 0xd800) {
		print_unicode_error(vformat("Unpaired surrogate (%x)", (uint32_t)p_char));
		dst[0] = _replacement_char;
	} else if (p_char > 0x10ffff) {
		print_unicode_error(vformat("Invalid unicode codepoint (%x)", (uint32_t)p_char));
		dst[0] = _replacement_char;
	} else {
		dst[0] = p_char;
	}

	dst[1] = 0;
}

// assumes the following have already been validated:
// p_char != nullptr
// p_length > 0
// p_length <= p_char strlen
void String::copy_from_unchecked(const char32_t *p_char, const int p_length) {
	resize(p_length + 1);

	const char32_t *end = p_char + p_length;
	char32_t *dst = ptrw();

	for (; p_char < end; ++p_char, ++dst) {
		const char32_t chr = *p_char;
		if ((chr & 0xfffff800) == 0xd800) {
			print_unicode_error(vformat("Unpaired surrogate (%x)", (uint32_t)chr));
			*dst = _replacement_char;
			continue;
		}
		if (chr > 0x10ffff) {
			print_unicode_error(vformat("Invalid unicode codepoint (%x)", (uint32_t)chr));
			*dst = _replacement_char;
			continue;
		}
		*dst = chr;
	}
	*dst = 0;
}

String String::operator+(const String &p_str) const {
	String res = *this;
	res += p_str;
	return res;
}

String String::operator+(char32_t p_char) const {
	String res = *this;
	res += p_char;
	return res;
}

String operator+(const char *p_chr, const String &p_str) {
	String tmp = p_chr;
	tmp += p_str;
	return tmp;
}

String operator+(const wchar_t *p_chr, const String &p_str) {
#ifdef WINDOWS_ENABLED
	// wchar_t is 16-bit
	String tmp = String::utf16((const char16_t *)p_chr);
#else
	// wchar_t is 32-bit
	String tmp = (const char32_t *)p_chr;
#endif
	tmp += p_str;
	return tmp;
}

String operator+(char32_t p_chr, const String &p_str) {
	return (String::chr(p_chr) + p_str);
}

String &String::operator+=(const String &p_str) {
	const int lhs_len = length();
	if (lhs_len == 0) {
		*this = p_str;
		return *this;
	}

	const int rhs_len = p_str.length();
	if (rhs_len == 0) {
		return *this;
	}

	resize(lhs_len + rhs_len + 1);

	const char32_t *src = p_str.ptr();
	char32_t *dst = ptrw() + lhs_len;

	// Don't copy the terminating null with `memcpy` to avoid undefined behavior when string is being added to itself (it would overlap the destination).
	memcpy(dst, src, rhs_len * sizeof(char32_t));
	*(dst + rhs_len) = _null;

	return *this;
}

String &String::operator+=(const char *p_str) {
	if (!p_str || p_str[0] == 0) {
		return *this;
	}

	const int lhs_len = length();
	const size_t rhs_len = strlen(p_str);

	resize(lhs_len + rhs_len + 1);

	char32_t *dst = ptrw() + lhs_len;

	for (size_t i = 0; i <= rhs_len; i++) {
#if CHAR_MIN == 0
		uint8_t c = p_str[i];
#else
		uint8_t c = p_str[i] >= 0 ? p_str[i] : uint8_t(256 + p_str[i]);
#endif
		if (c == 0 && i < rhs_len) {
			print_unicode_error("NUL character", true);
			dst[i] = _replacement_char;
		} else {
			dst[i] = c;
		}
	}

	return *this;
}

String &String::operator+=(const wchar_t *p_str) {
#ifdef WINDOWS_ENABLED
	// wchar_t is 16-bit
	*this += String::utf16((const char16_t *)p_str);
#else
	// wchar_t is 32-bit
	*this += String((const char32_t *)p_str);
#endif
	return *this;
}

String &String::operator+=(const char32_t *p_str) {
	*this += String(p_str);
	return *this;
}

String &String::operator+=(char32_t p_char) {
	if (p_char == 0) {
		print_unicode_error("NUL character", true);
		return *this;
	}

	const int lhs_len = length();
	resize(lhs_len + 2);
	char32_t *dst = ptrw();

	if ((p_char & 0xfffff800) == 0xd800) {
		print_unicode_error(vformat("Unpaired surrogate (%x)", (uint32_t)p_char));
		dst[lhs_len] = _replacement_char;
	} else if (p_char > 0x10ffff) {
		print_unicode_error(vformat("Invalid unicode codepoint (%x)", (uint32_t)p_char));
		dst[lhs_len] = _replacement_char;
	} else {
		dst[lhs_len] = p_char;
	}

	dst[lhs_len + 1] = 0;

	return *this;
}

bool String::operator==(const char *p_str) const {
	// compare Latin-1 encoded c-string
	int len = strlen(p_str);

	if (length() != len) {
		return false;
	}
	if (is_empty()) {
		return true;
	}

	int l = length();

	const char32_t *dst = get_data();

	// Compare char by char
	for (int i = 0; i < l; i++) {
		if ((char32_t)p_str[i] != dst[i]) {
			return false;
		}
	}

	return true;
}

bool String::operator==(const wchar_t *p_str) const {
#ifdef WINDOWS_ENABLED
	// wchar_t is 16-bit, parse as UTF-16
	return *this == String::utf16((const char16_t *)p_str);
#else
	// wchar_t is 32-bit, compare char by char
	return *this == (const char32_t *)p_str;
#endif
}

bool String::operator==(const char32_t *p_str) const {
	const int len = strlen(p_str);

	if (length() != len) {
		return false;
	}
	if (is_empty()) {
		return true;
	}

	return memcmp(ptr(), p_str, len * sizeof(char32_t)) == 0;
}

bool String::operator==(const String &p_str) const {
	if (length() != p_str.length()) {
		return false;
	}
	if (is_empty()) {
		return true;
	}

	return memcmp(ptr(), p_str.ptr(), length() * sizeof(char32_t)) == 0;
}

bool String::operator==(const StrRange<char32_t> &p_str_range) const {
	const int len = p_str_range.len;

	if (length() != len) {
		return false;
	}
	if (is_empty()) {
		return true;
	}

	return memcmp(ptr(), p_str_range.c_str, len * sizeof(char32_t)) == 0;
}

bool operator==(const char *p_chr, const String &p_str) {
	return p_str == p_chr;
}

bool operator==(const wchar_t *p_chr, const String &p_str) {
#ifdef WINDOWS_ENABLED
	// wchar_t is 16-bit
	return p_str == String::utf16((const char16_t *)p_chr);
#else
	// wchar_t is 32-bi
	return p_str == String((const char32_t *)p_chr);
#endif
}

bool operator!=(const char *p_chr, const String &p_str) {
	return !(p_str == p_chr);
}

bool operator!=(const wchar_t *p_chr, const String &p_str) {
#ifdef WINDOWS_ENABLED
	// wchar_t is 16-bit
	return !(p_str == String::utf16((const char16_t *)p_chr));
#else
	// wchar_t is 32-bi
	return !(p_str == String((const char32_t *)p_chr));
#endif
}

bool String::operator!=(const char *p_str) const {
	return (!(*this == p_str));
}

bool String::operator!=(const wchar_t *p_str) const {
	return (!(*this == p_str));
}

bool String::operator!=(const char32_t *p_str) const {
	return (!(*this == p_str));
}

bool String::operator!=(const String &p_str) const {
	return !((*this == p_str));
}

bool String::operator<=(const String &p_str) const {
	return !(p_str < *this);
}

bool String::operator>(const String &p_str) const {
	return p_str < *this;
}

bool String::operator>=(const String &p_str) const {
	return !(*this < p_str);
}

bool String::operator<(const char *p_str) const {
	if (is_empty() && p_str[0] == 0) {
		return false;
	}
	if (is_empty()) {
		return true;
	}
	return is_str_less(get_data(), p_str);
}

bool String::operator<(const wchar_t *p_str) const {
	if (is_empty() && p_str[0] == 0) {
		return false;
	}
	if (is_empty()) {
		return true;
	}

#ifdef WINDOWS_ENABLED
	// wchar_t is 16-bit
	return is_str_less(get_data(), String::utf16((const char16_t *)p_str).get_data());
#else
	// wchar_t is 32-bit
	return is_str_less(get_data(), (const char32_t *)p_str);
#endif
}

bool String::operator<(const char32_t *p_str) const {
	if (is_empty() && p_str[0] == 0) {
		return false;
	}
	if (is_empty()) {
		return true;
	}

	return is_str_less(get_data(), p_str);
}

bool String::operator<(const String &p_str) const {
	return operator<(p_str.get_data());
}

signed char String::nocasecmp_to(const String &p_str) const {
	if (is_empty() && p_str.is_empty()) {
		return 0;
	}
	if (is_empty()) {
		return -1;
	}
	if (p_str.is_empty()) {
		return 1;
	}

	const char32_t *that_str = p_str.get_data();
	const char32_t *this_str = get_data();

	while (true) {
		if (*that_str == 0 && *this_str == 0) { // If both strings are at the end, they are equal.
			return 0;
		} else if (*this_str == 0) { // If at the end of this, and not of other, we are less.
			return -1;
		} else if (*that_str == 0) { // If at end of other, and not of this, we are greater.
			return 1;
		} else if (_find_upper(*this_str) < _find_upper(*that_str)) { // If current character in this is less, we are less.
			return -1;
		} else if (_find_upper(*this_str) > _find_upper(*that_str)) { // If current character in this is greater, we are greater.
			return 1;
		}

		this_str++;
		that_str++;
	}
}

signed char String::casecmp_to(const String &p_str) const {
	if (is_empty() && p_str.is_empty()) {
		return 0;
	}
	if (is_empty()) {
		return -1;
	}
	if (p_str.is_empty()) {
		return 1;
	}

	const char32_t *that_str = p_str.get_data();
	const char32_t *this_str = get_data();

	while (true) {
		if (*that_str == 0 && *this_str == 0) { // If both strings are at the end, they are equal.
			return 0;
		} else if (*this_str == 0) { // If at the end of this, and not of other, we are less.
			return -1;
		} else if (*that_str == 0) { // If at end of other, and not of this, we are greater.
			return 1;
		} else if (*this_str < *that_str) { // If current character in this is less, we are less.
			return -1;
		} else if (*this_str > *that_str) { // If current character in this is greater, we are greater.
			return 1;
		}

		this_str++;
		that_str++;
	}
}

static _FORCE_INLINE_ signed char natural_cmp_common(const char32_t *&r_this_str, const char32_t *&r_that_str) {
	// Keep ptrs to start of numerical sequences.
	const char32_t *this_substr = r_this_str;
	const char32_t *that_substr = r_that_str;

	// Compare lengths of both numerical sequences, ignoring leading zeros.
	while (is_digit(*r_this_str)) {
		r_this_str++;
	}
	while (is_digit(*r_that_str)) {
		r_that_str++;
	}
	while (*this_substr == '0') {
		this_substr++;
	}
	while (*that_substr == '0') {
		that_substr++;
	}
	int this_len = r_this_str - this_substr;
	int that_len = r_that_str - that_substr;

	if (this_len < that_len) {
		return -1;
	} else if (this_len > that_len) {
		return 1;
	}

	// If lengths equal, compare lexicographically.
	while (this_substr != r_this_str && that_substr != r_that_str) {
		if (*this_substr < *that_substr) {
			return -1;
		} else if (*this_substr > *that_substr) {
			return 1;
		}
		this_substr++;
		that_substr++;
	}

	return 0;
}

static _FORCE_INLINE_ signed char naturalcasecmp_to_base(const char32_t *p_this_str, const char32_t *p_that_str) {
	if (p_this_str && p_that_str) {
		while (*p_this_str == '.' || *p_that_str == '.') {
			if (*p_this_str++ != '.') {
				return 1;
			}
			if (*p_that_str++ != '.') {
				return -1;
			}
			if (!*p_that_str) {
				return 1;
			}
			if (!*p_this_str) {
				return -1;
			}
		}

		while (*p_this_str) {
			if (!*p_that_str) {
				return 1;
			} else if (is_digit(*p_this_str)) {
				if (!is_digit(*p_that_str)) {
					return -1;
				}

				signed char ret = natural_cmp_common(p_this_str, p_that_str);
				if (ret) {
					return ret;
				}
			} else if (is_digit(*p_that_str)) {
				return 1;
			} else {
				if (*p_this_str < *p_that_str) { // If current character in this is less, we are less.
					return -1;
				} else if (*p_this_str > *p_that_str) { // If current character in this is greater, we are greater.
					return 1;
				}

				p_this_str++;
				p_that_str++;
			}
		}
		if (*p_that_str) {
			return -1;
		}
	}

	return 0;
}

signed char String::naturalcasecmp_to(const String &p_str) const {
	const char32_t *this_str = get_data();
	const char32_t *that_str = p_str.get_data();

	return naturalcasecmp_to_base(this_str, that_str);
}

static _FORCE_INLINE_ signed char naturalnocasecmp_to_base(const char32_t *p_this_str, const char32_t *p_that_str) {
	if (p_this_str && p_that_str) {
		while (*p_this_str == '.' || *p_that_str == '.') {
			if (*p_this_str++ != '.') {
				return 1;
			}
			if (*p_that_str++ != '.') {
				return -1;
			}
			if (!*p_that_str) {
				return 1;
			}
			if (!*p_this_str) {
				return -1;
			}
		}

		while (*p_this_str) {
			if (!*p_that_str) {
				return 1;
			} else if (is_digit(*p_this_str)) {
				if (!is_digit(*p_that_str)) {
					return -1;
				}

				signed char ret = natural_cmp_common(p_this_str, p_that_str);
				if (ret) {
					return ret;
				}
			} else if (is_digit(*p_that_str)) {
				return 1;
			} else {
				if (_find_upper(*p_this_str) < _find_upper(*p_that_str)) { // If current character in this is less, we are less.
					return -1;
				} else if (_find_upper(*p_this_str) > _find_upper(*p_that_str)) { // If current character in this is greater, we are greater.
					return 1;
				}

				p_this_str++;
				p_that_str++;
			}
		}
		if (*p_that_str) {
			return -1;
		}
	}

	return 0;
}

signed char String::naturalnocasecmp_to(const String &p_str) const {
	const char32_t *this_str = get_data();
	const char32_t *that_str = p_str.get_data();

	return naturalnocasecmp_to_base(this_str, that_str);
}

static _FORCE_INLINE_ signed char file_cmp_common(const char32_t *&r_this_str, const char32_t *&r_that_str) {
	// Compare leading `_` sequences.
	while ((*r_this_str == '_' && *r_that_str) || (*r_this_str && *r_that_str == '_')) {
		// Sort `_` lower than everything except `.`
		if (*r_this_str != '_') {
			return *r_this_str == '.' ? -1 : 1;
		} else if (*r_that_str != '_') {
			return *r_that_str == '.' ? 1 : -1;
		}
		r_this_str++;
		r_that_str++;
	}

	return 0;
}

signed char String::filecasecmp_to(const String &p_str) const {
	const char32_t *this_str = get_data();
	const char32_t *that_str = p_str.get_data();

	signed char ret = file_cmp_common(this_str, that_str);
	if (ret) {
		return ret;
	}

	return naturalcasecmp_to_base(this_str, that_str);
}

signed char String::filenocasecmp_to(const String &p_str) const {
	const char32_t *this_str = get_data();
	const char32_t *that_str = p_str.get_data();

	signed char ret = file_cmp_common(this_str, that_str);
	if (ret) {
		return ret;
	}

	return naturalnocasecmp_to_base(this_str, that_str);
}

const char32_t *String::get_data() const {
	static const char32_t zero = 0;
	return size() ? &operator[](0) : &zero;
}

String String::_camelcase_to_underscore() const {
	const char32_t *cstr = get_data();
	String new_string;
	int start_index = 0;

	if (length() == 0) {
		return *this;
	}

	bool is_prev_upper = is_unicode_upper_case(cstr[0]);
	bool is_prev_lower = is_unicode_lower_case(cstr[0]);
	bool is_prev_digit = is_digit(cstr[0]);

	for (int i = 1; i < length(); i++) {
		const bool is_curr_upper = is_unicode_upper_case(cstr[i]);
		const bool is_curr_lower = is_unicode_lower_case(cstr[i]);
		const bool is_curr_digit = is_digit(cstr[i]);

		bool is_next_lower = false;
		if (i + 1 < length()) {
			is_next_lower = is_unicode_lower_case(cstr[i + 1]);
		}

		const bool cond_a = is_prev_lower && is_curr_upper; // aA
		const bool cond_b = (is_prev_upper || is_prev_digit) && is_curr_upper && is_next_lower; // AAa, 2Aa
		const bool cond_c = is_prev_digit && is_curr_lower && is_next_lower; // 2aa
		const bool cond_d = (is_prev_upper || is_prev_lower) && is_curr_digit; // A2, a2

		if (cond_a || cond_b || cond_c || cond_d) {
			new_string += substr(start_index, i - start_index) + "_";
			start_index = i;
		}

		is_prev_upper = is_curr_upper;
		is_prev_lower = is_curr_lower;
		is_prev_digit = is_curr_digit;
	}

	new_string += substr(start_index, size() - start_index);
	return new_string.to_lower();
}

String String::capitalize() const {
	String aux = _camelcase_to_underscore().replace("_", " ").strip_edges();
	String cap;
	for (int i = 0; i < aux.get_slice_count(" "); i++) {
		String slice = aux.get_slicec(' ', i);
		if (slice.length() > 0) {
			slice[0] = _find_upper(slice[0]);
			if (i > 0) {
				cap += " ";
			}
			cap += slice;
		}
	}

	return cap;
}

String String::to_camel_case() const {
	String s = to_pascal_case();
	if (!s.is_empty()) {
		s[0] = _find_lower(s[0]);
	}
	return s;
}

String String::to_pascal_case() const {
	return capitalize().replace(" ", "");
}

String String::to_snake_case() const {
	return _camelcase_to_underscore().replace(" ", "_").strip_edges();
}

String String::get_with_code_lines() const {
	const Vector<String> lines = split("\n");
	String ret;
	for (int i = 0; i < lines.size(); i++) {
		if (i > 0) {
			ret += "\n";
		}
		ret += vformat("%4d | %s", i + 1, lines[i]);
	}
	return ret;
}

int String::get_slice_count(const String &p_splitter) const {
	if (is_empty()) {
		return 0;
	}
	if (p_splitter.is_empty()) {
		return 0;
	}

	int pos = 0;
	int slices = 1;

	while ((pos = find(p_splitter, pos)) >= 0) {
		slices++;
		pos += p_splitter.length();
	}

	return slices;
}

int String::get_slice_count(const char *p_splitter) const {
	if (is_empty()) {
		return 0;
	}
	if (p_splitter == nullptr || *p_splitter == '\0') {
		return 0;
	}

	int pos = 0;
	int slices = 1;
	int splitter_length = strlen(p_splitter);

	while ((pos = find(p_splitter, pos)) >= 0) {
		slices++;
		pos += splitter_length;
	}

	return slices;
}

String String::get_slice(const String &p_splitter, int p_slice) const {
	if (is_empty() || p_splitter.is_empty()) {
		return "";
	}

	int pos = 0;
	int prev_pos = 0;
	//int slices=1;
	if (p_slice < 0) {
		return "";
	}
	if (find(p_splitter) == -1) {
		return *this;
	}

	int i = 0;
	while (true) {
		pos = find(p_splitter, pos);
		if (pos == -1) {
			pos = length(); //reached end
		}

		int from = prev_pos;
		//int to=pos;

		if (p_slice == i) {
			return substr(from, pos - from);
		}

		if (pos == length()) { //reached end and no find
			break;
		}
		pos += p_splitter.length();
		prev_pos = pos;
		i++;
	}

	return ""; //no find!
}

String String::get_slice(const char *p_splitter, int p_slice) const {
	if (is_empty() || p_splitter == nullptr || *p_splitter == '\0') {
		return "";
	}

	int pos = 0;
	int prev_pos = 0;
	//int slices=1;
	if (p_slice < 0) {
		return "";
	}
	if (find(p_splitter) == -1) {
		return *this;
	}

	int i = 0;
	const int splitter_length = strlen(p_splitter);
	while (true) {
		pos = find(p_splitter, pos);
		if (pos == -1) {
			pos = length(); //reached end
		}

		int from = prev_pos;
		//int to=pos;

		if (p_slice == i) {
			return substr(from, pos - from);
		}

		if (pos == length()) { //reached end and no find
			break;
		}
		pos += splitter_length;
		prev_pos = pos;
		i++;
	}

	return ""; //no find!
}

String String::get_slicec(char32_t p_splitter, int p_slice) const {
	if (is_empty()) {
		return String();
	}

	if (p_slice < 0) {
		return String();
	}

	const char32_t *c = ptr();
	int i = 0;
	int prev = 0;
	int count = 0;
	while (true) {
		if (c[i] == 0 || c[i] == p_splitter) {
			if (p_slice == count) {
				return substr(prev, i - prev);
			} else if (c[i] == 0) {
				return String();
			} else {
				count++;
				prev = i + 1;
			}
		}

		i++;
	}
}

Vector<String> String::split_spaces() const {
	Vector<String> ret;
	int from = 0;
	int i = 0;
	int len = length();
	if (len == 0) {
		return ret;
	}

	bool inside = false;

	while (true) {
		bool empty = operator[](i) < 33;

		if (i == 0) {
			inside = !empty;
		}

		if (!empty && !inside) {
			inside = true;
			from = i;
		}

		if (empty && inside) {
			ret.push_back(substr(from, i - from));
			inside = false;
		}

		if (i == len) {
			break;
		}
		i++;
	}

	return ret;
}

Vector<String> String::split(const String &p_splitter, bool p_allow_empty, int p_maxsplit) const {
	Vector<String> ret;

	if (is_empty()) {
		if (p_allow_empty) {
			ret.push_back("");
		}
		return ret;
	}

	int from = 0;
	int len = length();

	while (true) {
		int end;
		if (p_splitter.is_empty()) {
			end = from + 1;
		} else {
			end = find(p_splitter, from);
			if (end < 0) {
				end = len;
			}
		}
		if (p_allow_empty || (end > from)) {
			if (p_maxsplit <= 0) {
				ret.push_back(substr(from, end - from));
			} else {
				// Put rest of the string and leave cycle.
				if (p_maxsplit == ret.size()) {
					ret.push_back(substr(from, len));
					break;
				}

				// Otherwise, push items until positive limit is reached.
				ret.push_back(substr(from, end - from));
			}
		}

		if (end == len) {
			break;
		}

		from = end + p_splitter.length();
	}

	return ret;
}

Vector<String> String::split(const char *p_splitter, bool p_allow_empty, int p_maxsplit) const {
	Vector<String> ret;

	if (is_empty()) {
		if (p_allow_empty) {
			ret.push_back("");
		}
		return ret;
	}

	int from = 0;
	int len = length();
	const int splitter_length = strlen(p_splitter);

	while (true) {
		int end;
		if (p_splitter == nullptr || *p_splitter == '\0') {
			end = from + 1;
		} else {
			end = find(p_splitter, from);
			if (end < 0) {
				end = len;
			}
		}
		if (p_allow_empty || (end > from)) {
			if (p_maxsplit <= 0) {
				ret.push_back(substr(from, end - from));
			} else {
				// Put rest of the string and leave cycle.
				if (p_maxsplit == ret.size()) {
					ret.push_back(substr(from, len));
					break;
				}

				// Otherwise, push items until positive limit is reached.
				ret.push_back(substr(from, end - from));
			}
		}

		if (end == len) {
			break;
		}

		from = end + splitter_length;
	}

	return ret;
}

Vector<String> String::rsplit(const String &p_splitter, bool p_allow_empty, int p_maxsplit) const {
	Vector<String> ret;
	const int len = length();
	int remaining_len = len;

	while (true) {
		if (remaining_len < p_splitter.length() || (p_maxsplit > 0 && p_maxsplit == ret.size())) {
			// no room for another splitter or hit max splits, push what's left and we're done
			if (p_allow_empty || remaining_len > 0) {
				ret.push_back(substr(0, remaining_len));
			}
			break;
		}

		int left_edge;
		if (p_splitter.is_empty()) {
			left_edge = remaining_len - 1;
			if (left_edge == 0) {
				left_edge--; // Skip to the < 0 condition.
			}
		} else {
			left_edge = rfind(p_splitter, remaining_len - p_splitter.length());
		}

		if (left_edge < 0) {
			// no more splitters, we're done
			ret.push_back(substr(0, remaining_len));
			break;
		}

		int substr_start = left_edge + p_splitter.length();
		if (p_allow_empty || substr_start < remaining_len) {
			ret.push_back(substr(substr_start, remaining_len - substr_start));
		}

		remaining_len = left_edge;
	}

	ret.reverse();
	return ret;
}

Vector<String> String::rsplit(const char *p_splitter, bool p_allow_empty, int p_maxsplit) const {
	Vector<String> ret;
	const int len = length();
	const int splitter_length = strlen(p_splitter);
	int remaining_len = len;

	while (true) {
		if (remaining_len < splitter_length || (p_maxsplit > 0 && p_maxsplit == ret.size())) {
			// no room for another splitter or hit max splits, push what's left and we're done
			if (p_allow_empty || remaining_len > 0) {
				ret.push_back(substr(0, remaining_len));
			}
			break;
		}

		int left_edge;
		if (p_splitter == nullptr || *p_splitter == '\0') {
			left_edge = remaining_len - 1;
			if (left_edge == 0) {
				left_edge--; // Skip to the < 0 condition.
			}
		} else {
			left_edge = rfind(p_splitter, remaining_len - splitter_length);
		}

		if (left_edge < 0) {
			// no more splitters, we're done
			ret.push_back(substr(0, remaining_len));
			break;
		}

		int substr_start = left_edge + splitter_length;
		if (p_allow_empty || substr_start < remaining_len) {
			ret.push_back(substr(substr_start, remaining_len - substr_start));
		}

		remaining_len = left_edge;
	}

	ret.reverse();
	return ret;
}

Vector<double> String::split_floats(const String &p_splitter, bool p_allow_empty) const {
	Vector<double> ret;
	int from = 0;
	int len = length();

	String buffer = *this;
	while (true) {
		int end = find(p_splitter, from);
		if (end < 0) {
			end = len;
		}
		if (p_allow_empty || (end > from)) {
			buffer[end] = 0;
			ret.push_back(String::to_float(&buffer.get_data()[from]));
			buffer[end] = _cowdata.get(end);
		}

		if (end == len) {
			break;
		}

		from = end + p_splitter.length();
	}

	return ret;
}

Vector<float> String::split_floats_mk(const Vector<String> &p_splitters, bool p_allow_empty) const {
	Vector<float> ret;
	int from = 0;
	int len = length();

	String buffer = *this;
	while (true) {
		int idx;
		int end = findmk(p_splitters, from, &idx);
		int spl_len = 1;
		if (end < 0) {
			end = len;
		} else {
			spl_len = p_splitters[idx].length();
		}

		if (p_allow_empty || (end > from)) {
			buffer[end] = 0;
			ret.push_back(String::to_float(&buffer.get_data()[from]));
			buffer[end] = _cowdata.get(end);
		}

		if (end == len) {
			break;
		}

		from = end + spl_len;
	}

	return ret;
}

Vector<int> String::split_ints(const String &p_splitter, bool p_allow_empty) const {
	Vector<int> ret;
	int from = 0;
	int len = length();

	while (true) {
		int end = find(p_splitter, from);
		if (end < 0) {
			end = len;
		}
		if (p_allow_empty || (end > from)) {
			ret.push_back(String::to_int(&get_data()[from], end - from));
		}

		if (end == len) {
			break;
		}

		from = end + p_splitter.length();
	}

	return ret;
}

Vector<int> String::split_ints_mk(const Vector<String> &p_splitters, bool p_allow_empty) const {
	Vector<int> ret;
	int from = 0;
	int len = length();

	while (true) {
		int idx;
		int end = findmk(p_splitters, from, &idx);
		int spl_len = 1;
		if (end < 0) {
			end = len;
		} else {
			spl_len = p_splitters[idx].length();
		}

		if (p_allow_empty || (end > from)) {
			ret.push_back(String::to_int(&get_data()[from], end - from));
		}

		if (end == len) {
			break;
		}

		from = end + spl_len;
	}

	return ret;
}

String String::join(const Vector<String> &parts) const {
	if (parts.is_empty()) {
		return String();
	} else if (parts.size() == 1) {
		return parts[0];
	}

	const int this_length = length();

	int new_size = (parts.size() - 1) * this_length;
	for (const String &part : parts) {
		new_size += part.length();
	}
	new_size += 1;

	String ret;
	ret.resize(new_size);
	char32_t *ret_ptrw = ret.ptrw();
	const char32_t *this_ptr = ptr();

	bool first = true;
	for (const String &part : parts) {
		if (first) {
			first = false;
		} else if (this_length) {
			memcpy(ret_ptrw, this_ptr, this_length * sizeof(char32_t));
			ret_ptrw += this_length;
		}

		const int part_length = part.length();
		if (part_length) {
			memcpy(ret_ptrw, part.ptr(), part_length * sizeof(char32_t));
			ret_ptrw += part_length;
		}
	}

	*ret_ptrw = 0;

	return ret;
}

char32_t String::char_uppercase(char32_t p_char) {
	return _find_upper(p_char);
}

char32_t String::char_lowercase(char32_t p_char) {
	return _find_lower(p_char);
}

String String::to_upper() const {
	if (is_empty()) {
		return *this;
	}

	String upper;
	upper.resize(size());
	const char32_t *old_ptr = ptr();
	char32_t *upper_ptrw = upper.ptrw();

	while (*old_ptr) {
		*upper_ptrw++ = _find_upper(*old_ptr++);
	}

	*upper_ptrw = 0;

	return upper;
}

String String::to_lower() const {
	if (is_empty()) {
		return *this;
	}

	String lower;
	lower.resize(size());
	const char32_t *old_ptr = ptr();
	char32_t *lower_ptrw = lower.ptrw();

	while (*old_ptr) {
		*lower_ptrw++ = _find_lower(*old_ptr++);
	}

	*lower_ptrw = 0;

	return lower;
}

String String::chr(char32_t p_char) {
	char32_t c[2] = { p_char, 0 };
	return String(c);
}

String String::num(double p_num, int p_decimals) {
	if (Math::is_nan(p_num)) {
		return "nan";
	}

	if (Math::is_inf(p_num)) {
		if (signbit(p_num)) {
			return "-inf";
		} else {
			return "inf";
		}
	}

	if (p_decimals < 0) {
		p_decimals = 14;
		const double abs_num = Math::abs(p_num);
		if (abs_num > 10) {
			// We want to align the digits to the above reasonable default, so we only
			// need to subtract log10 for numbers with a positive power of ten.
			p_decimals -= (int)floor(log10(abs_num));
		}
	}
	if (p_decimals > MAX_DECIMALS) {
		p_decimals = MAX_DECIMALS;
	}

	char fmt[7];
	fmt[0] = '%';
	fmt[1] = '.';

	if (p_decimals < 0) {
		fmt[1] = 'l';
		fmt[2] = 'f';
		fmt[3] = 0;
	} else if (p_decimals < 10) {
		fmt[2] = '0' + p_decimals;
		fmt[3] = 'l';
		fmt[4] = 'f';
		fmt[5] = 0;
	} else {
		fmt[2] = '0' + (p_decimals / 10);
		fmt[3] = '0' + (p_decimals % 10);
		fmt[4] = 'l';
		fmt[5] = 'f';
		fmt[6] = 0;
	}
	// if we want to convert a double with as much decimal places as
	// DBL_MAX or DBL_MIN then we would theoretically need a buffer of at least
	// DBL_MAX_10_EXP + 2 for DBL_MAX and DBL_MAX_10_EXP + 4 for DBL_MIN.
	// BUT those values where still giving me exceptions, so I tested from
	// DBL_MAX_10_EXP + 10 incrementing one by one and DBL_MAX_10_EXP + 17 (325)
	// was the first buffer size not to throw an exception
	char buf[325];

#if defined(__GNUC__) || defined(_MSC_VER)
	// PLEASE NOTE that, albeit vcrt online reference states that snprintf
	// should safely truncate the output to the given buffer size, we have
	// found a case where this is not true, so we should create a buffer
	// as big as needed
	snprintf(buf, 325, fmt, p_num);
#else
	sprintf(buf, fmt, p_num);
#endif

	buf[324] = 0;
	// Destroy trailing zeroes, except one after period.
	{
		bool period = false;
		int z = 0;
		while (buf[z]) {
			if (buf[z] == '.') {
				period = true;
			}
			z++;
		}

		if (period) {
			z--;
			while (z > 0) {
				if (buf[z] == '0') {
					buf[z] = 0;
				} else if (buf[z] == '.') {
					buf[z + 1] = '0';
					break;
				} else {
					break;
				}

				z--;
			}
		}
	}

	return buf;
}

String String::num_int64(int64_t p_num, int base, bool capitalize_hex) {
	ERR_FAIL_COND_V_MSG(base < 2 || base > 36, "", "Cannot convert to base " + itos(base) + ", since the value is " + (base < 2 ? "less than 2." : "greater than 36."));

	bool sign = p_num < 0;

	int64_t n = p_num;

	int chars = 0;
	do {
		n /= base;
		chars++;
	} while (n);

	if (sign) {
		chars++;
	}
	String s;
	s.resize(chars + 1);
	char32_t *c = s.ptrw();
	c[chars] = 0;
	n = p_num;
	do {
		int mod = ABS(n % base);
		if (mod >= 10) {
			char a = (capitalize_hex ? 'A' : 'a');
			c[--chars] = a + (mod - 10);
		} else {
			c[--chars] = '0' + mod;
		}

		n /= base;
	} while (n);

	if (sign) {
		c[0] = '-';
	}

	return s;
}

String String::num_uint64(uint64_t p_num, int base, bool capitalize_hex) {
	ERR_FAIL_COND_V_MSG(base < 2 || base > 36, "", "Cannot convert to base " + itos(base) + ", since the value is " + (base < 2 ? "less than 2." : "greater than 36."));

	uint64_t n = p_num;

	int chars = 0;
	do {
		n /= base;
		chars++;
	} while (n);

	String s;
	s.resize(chars + 1);
	char32_t *c = s.ptrw();
	c[chars] = 0;
	n = p_num;
	do {
		int mod = n % base;
		if (mod >= 10) {
			char a = (capitalize_hex ? 'A' : 'a');
			c[--chars] = a + (mod - 10);
		} else {
			c[--chars] = '0' + mod;
		}

		n /= base;
	} while (n);

	return s;
}

String String::num_real(double p_num, bool p_trailing) {
	if (Math::is_nan(p_num) || Math::is_inf(p_num)) {
		return num(p_num, 0);
	}

	if (p_num == (double)(int64_t)p_num) {
		if (p_trailing) {
			return num_int64((int64_t)p_num) + ".0";
		} else {
			return num_int64((int64_t)p_num);
		}
	}

	int decimals = 14;
	// We want to align the digits to the above sane default, so we only need
	// to subtract log10 for numbers with a positive power of ten magnitude.
	const double abs_num = Math::abs(p_num);
	if (abs_num > 10) {
		decimals -= (int)floor(log10(abs_num));
	}

	return num(p_num, decimals);
}

String String::num_real(float p_num, bool p_trailing) {
	if (Math::is_nan(p_num) || Math::is_inf(p_num)) {
		return num(p_num, 0);
	}

	if (p_num == (float)(int64_t)p_num) {
		if (p_trailing) {
			return num_int64((int64_t)p_num) + ".0";
		} else {
			return num_int64((int64_t)p_num);
		}
	}
	int decimals = 6;
	// We want to align the digits to the above sane default, so we only need
	// to subtract log10 for numbers with a positive power of ten magnitude.
	const float abs_num = Math::abs(p_num);
	if (abs_num > 10) {
		decimals -= (int)floor(log10(abs_num));
	}
	return num(p_num, decimals);
}

String String::num_scientific(double p_num) {
	if (Math::is_nan(p_num) || Math::is_inf(p_num)) {
		return num(p_num, 0);
	}

	char buf[256];

#if defined(__GNUC__) || defined(_MSC_VER)

#if defined(__MINGW32__) && defined(_TWO_DIGIT_EXPONENT) && !defined(_UCRT)
	// MinGW requires _set_output_format() to conform to C99 output for printf
	unsigned int old_exponent_format = _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
	snprintf(buf, 256, "%lg", p_num);

#if defined(__MINGW32__) && defined(_TWO_DIGIT_EXPONENT) && !defined(_UCRT)
	_set_output_format(old_exponent_format);
#endif

#else
	sprintf(buf, "%.16lg", p_num);
#endif

	buf[255] = 0;

	return buf;
}

String String::md5(const uint8_t *p_md5) {
	return String::hex_encode_buffer(p_md5, 16);
}

String String::hex_encode_buffer(const uint8_t *p_buffer, int p_len) {
	static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	String ret;
	ret.resize(p_len * 2 + 1);
	char32_t *ret_ptrw = ret.ptrw();

	for (int i = 0; i < p_len; i++) {
		*ret_ptrw++ = hex[p_buffer[i] >> 4];
		*ret_ptrw++ = hex[p_buffer[i] & 0xF];
	}

	*ret_ptrw = 0;

	return ret;
}

Vector<uint8_t> String::hex_decode() const {
	ERR_FAIL_COND_V_MSG(length() % 2 != 0, Vector<uint8_t>(), "Hexadecimal string of uneven length.");

#define HEX_TO_BYTE(m_output, m_index)                                                                                   \
	uint8_t m_output;                                                                                                    \
	c = operator[](m_index);                                                                                             \
	if (is_digit(c)) {                                                                                                   \
		m_output = c - '0';                                                                                              \
	} else if (c >= 'a' && c <= 'f') {                                                                                   \
		m_output = c - 'a' + 10;                                                                                         \
	} else if (c >= 'A' && c <= 'F') {                                                                                   \
		m_output = c - 'A' + 10;                                                                                         \
	} else {                                                                                                             \
		ERR_FAIL_V_MSG(Vector<uint8_t>(), "Invalid hexadecimal character \"" + chr(c) + "\" at index " + m_index + "."); \
	}

	Vector<uint8_t> out;
	int len = length() / 2;
	out.resize(len);
	uint8_t *out_ptrw = out.ptrw();
	for (int i = 0; i < len; i++) {
		char32_t c;
		HEX_TO_BYTE(first, i * 2);
		HEX_TO_BYTE(second, i * 2 + 1);
		out_ptrw[i] = first * 16 + second;
	}
	return out;
#undef HEX_TO_BYTE
}

void String::print_unicode_error(const String &p_message, bool p_critical) const {
	if (p_critical) {
		print_error(vformat(U"Unicode parsing error, some characters were replaced with � (U+FFFD): %s", p_message));
	} else {
		print_error(vformat("Unicode parsing error: %s", p_message));
	}
}

CharString String::ascii(bool p_allow_extended) const {
	if (!length()) {
		return CharString();
	}

	CharString cs;
	cs.resize(size());
	char *cs_ptrw = cs.ptrw();
	const char32_t *this_ptr = ptr();

	for (int i = 0; i < size(); i++) {
		char32_t c = this_ptr[i];
		if ((c <= 0x7f) || (c <= 0xff && p_allow_extended)) {
			cs_ptrw[i] = char(c);
		} else {
			print_unicode_error(vformat("Invalid unicode codepoint (%x), cannot represent as ASCII/Latin-1", (uint32_t)c));
			cs_ptrw[i] = 0x20; // ASCII doesn't have a replacement character like unicode, 0x1a is sometimes used but is kinda arcane.
		}
	}

	return cs;
}

String String::utf8(const char *p_utf8, int p_len) {
	String ret;
	ret.parse_utf8(p_utf8, p_len);

	return ret;
}

Error String::parse_utf8(const char *p_utf8, int p_len, bool p_skip_cr) {
	if (!p_utf8) {
		return ERR_INVALID_DATA;
	}

	String aux;

	int cstr_size = 0;
	int str_size = 0;

	/* HANDLE BOM (Byte Order Mark) */
	if (p_len < 0 || p_len >= 3) {
		bool has_bom = uint8_t(p_utf8[0]) == 0xef && uint8_t(p_utf8[1]) == 0xbb && uint8_t(p_utf8[2]) == 0xbf;
		if (has_bom) {
			//8-bit encoding, byte order has no meaning in UTF-8, just skip it
			if (p_len >= 0) {
				p_len -= 3;
			}
			p_utf8 += 3;
		}
	}

	bool decode_error = false;
	bool decode_failed = false;
	{
		const char *ptrtmp = p_utf8;
		const char *ptrtmp_limit = p_len >= 0 ? &p_utf8[p_len] : nullptr;
		int skip = 0;
		uint8_t c_start = 0;
		while (ptrtmp != ptrtmp_limit && *ptrtmp) {
#if CHAR_MIN == 0
			uint8_t c = *ptrtmp;
#else
			uint8_t c = *ptrtmp >= 0 ? *ptrtmp : uint8_t(256 + *ptrtmp);
#endif

			if (skip == 0) {
				if (p_skip_cr && c == '\r') {
					ptrtmp++;
					continue;
				}
				/* Determine the number of characters in sequence */
				if ((c & 0x80) == 0) {
					skip = 0;
				} else if ((c & 0xe0) == 0xc0) {
					skip = 1;
				} else if ((c & 0xf0) == 0xe0) {
					skip = 2;
				} else if ((c & 0xf8) == 0xf0) {
					skip = 3;
				} else if ((c & 0xfc) == 0xf8) {
					skip = 4;
				} else if ((c & 0xfe) == 0xfc) {
					skip = 5;
				} else {
					skip = 0;
					print_unicode_error(vformat("Invalid UTF-8 leading byte (%x)", c), true);
					decode_failed = true;
				}
				c_start = c;

				if (skip == 1 && (c & 0x1e) == 0) {
					print_unicode_error(vformat("Overlong encoding (%x ...)", c));
					decode_error = true;
				}
				str_size++;
			} else {
				if ((c_start == 0xe0 && skip == 2 && c < 0xa0) || (c_start == 0xf0 && skip == 3 && c < 0x90) || (c_start == 0xf8 && skip == 4 && c < 0x88) || (c_start == 0xfc && skip == 5 && c < 0x84)) {
					print_unicode_error(vformat("Overlong encoding (%x %x ...)", c_start, c));
					decode_error = true;
				}
				if (c < 0x80 || c > 0xbf) {
					print_unicode_error(vformat("Invalid UTF-8 continuation byte (%x ... %x ...)", c_start, c), true);
					decode_failed = true;
					skip = 0;
				} else {
					--skip;
				}
			}

			cstr_size++;
			ptrtmp++;
		}

		if (skip) {
			print_unicode_error(vformat("Missing %d UTF-8 continuation byte(s)", skip), true);
			decode_failed = true;
		}
	}

	if (str_size == 0) {
		clear();
		return OK; // empty string
	}

	resize(str_size + 1);
	char32_t *dst = ptrw();
	dst[str_size] = 0;

	int skip = 0;
	uint32_t unichar = 0;
	while (cstr_size) {
#if CHAR_MIN == 0
		uint8_t c = *p_utf8;
#else
		uint8_t c = *p_utf8 >= 0 ? *p_utf8 : uint8_t(256 + *p_utf8);
#endif

		if (skip == 0) {
			if (p_skip_cr && c == '\r') {
				p_utf8++;
				continue;
			}
			/* Determine the number of characters in sequence */
			if ((c & 0x80) == 0) {
				*(dst++) = c;
				unichar = 0;
				skip = 0;
			} else if ((c & 0xe0) == 0xc0) {
				unichar = (0xff >> 3) & c;
				skip = 1;
			} else if ((c & 0xf0) == 0xe0) {
				unichar = (0xff >> 4) & c;
				skip = 2;
			} else if ((c & 0xf8) == 0xf0) {
				unichar = (0xff >> 5) & c;
				skip = 3;
			} else if ((c & 0xfc) == 0xf8) {
				unichar = (0xff >> 6) & c;
				skip = 4;
			} else if ((c & 0xfe) == 0xfc) {
				unichar = (0xff >> 7) & c;
				skip = 5;
			} else {
				*(dst++) = _replacement_char;
				unichar = 0;
				skip = 0;
			}
		} else {
			if (c < 0x80 || c > 0xbf) {
				*(dst++) = _replacement_char;
				skip = 0;
			} else {
				unichar = (unichar << 6) | (c & 0x3f);
				--skip;
				if (skip == 0) {
					if (unichar == 0) {
						print_unicode_error("NUL character", true);
						decode_failed = true;
						unichar = _replacement_char;
					} else if ((unichar & 0xfffff800) == 0xd800) {
						print_unicode_error(vformat("Unpaired surrogate (%x)", unichar), true);
						decode_failed = true;
						unichar = _replacement_char;
					} else if (unichar > 0x10ffff) {
						print_unicode_error(vformat("Invalid unicode codepoint (%x)", unichar), true);
						decode_failed = true;
						unichar = _replacement_char;
					}
					*(dst++) = unichar;
				}
			}
		}

		cstr_size--;
		p_utf8++;
	}
	if (skip) {
		*(dst++) = 0x20;
	}

	if (decode_failed) {
		return ERR_INVALID_DATA;
	} else if (decode_error) {
		return ERR_PARSE_ERROR;
	} else {
		return OK;
	}
}

CharString String::utf8() const {
	int l = length();
	if (!l) {
		return CharString();
	}

	const char32_t *d = &operator[](0);
	int fl = 0;
	for (int i = 0; i < l; i++) {
		uint32_t c = d[i];
		if (c <= 0x7f) { // 7 bits.
			fl += 1;
		} else if (c <= 0x7ff) { // 11 bits
			fl += 2;
		} else if (c <= 0xffff) { // 16 bits
			fl += 3;
		} else if (c <= 0x001fffff) { // 21 bits
			fl += 4;
		} else if (c <= 0x03ffffff) { // 26 bits
			fl += 5;
			print_unicode_error(vformat("Invalid unicode codepoint (%x)", c));
		} else if (c <= 0x7fffffff) { // 31 bits
			fl += 6;
			print_unicode_error(vformat("Invalid unicode codepoint (%x)", c));
		} else {
			fl += 1;
			print_unicode_error(vformat("Invalid unicode codepoint (%x), cannot represent as UTF-8", c), true);
		}
	}

	CharString utf8s;
	if (fl == 0) {
		return utf8s;
	}

	utf8s.resize(fl + 1);
	uint8_t *cdst = (uint8_t *)utf8s.get_data();

#define APPEND_CHAR(m_c) *(cdst++) = m_c

	for (int i = 0; i < l; i++) {
		uint32_t c = d[i];

		if (c <= 0x7f) { // 7 bits.
			APPEND_CHAR(c);
		} else if (c <= 0x7ff) { // 11 bits
			APPEND_CHAR(uint32_t(0xc0 | ((c >> 6) & 0x1f))); // Top 5 bits.
			APPEND_CHAR(uint32_t(0x80 | (c & 0x3f))); // Bottom 6 bits.
		} else if (c <= 0xffff) { // 16 bits
			APPEND_CHAR(uint32_t(0xe0 | ((c >> 12) & 0x0f))); // Top 4 bits.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 6) & 0x3f))); // Middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | (c & 0x3f))); // Bottom 6 bits.
		} else if (c <= 0x001fffff) { // 21 bits
			APPEND_CHAR(uint32_t(0xf0 | ((c >> 18) & 0x07))); // Top 3 bits.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 12) & 0x3f))); // Upper middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 6) & 0x3f))); // Lower middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | (c & 0x3f))); // Bottom 6 bits.
		} else if (c <= 0x03ffffff) { // 26 bits
			APPEND_CHAR(uint32_t(0xf8 | ((c >> 24) & 0x03))); // Top 2 bits.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 18) & 0x3f))); // Upper middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 12) & 0x3f))); // middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 6) & 0x3f))); // Lower middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | (c & 0x3f))); // Bottom 6 bits.
		} else if (c <= 0x7fffffff) { // 31 bits
			APPEND_CHAR(uint32_t(0xfc | ((c >> 30) & 0x01))); // Top 1 bit.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 24) & 0x3f))); // Upper upper middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 18) & 0x3f))); // Lower upper middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 12) & 0x3f))); // Upper lower middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | ((c >> 6) & 0x3f))); // Lower lower middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | (c & 0x3f))); // Bottom 6 bits.
		} else {
			// the string is a valid UTF32, so it should never happen ...
			print_unicode_error(vformat("Non scalar value (%x)", c), true);
			APPEND_CHAR(uint32_t(0xe0 | ((_replacement_char >> 12) & 0x0f))); // Top 4 bits.
			APPEND_CHAR(uint32_t(0x80 | ((_replacement_char >> 6) & 0x3f))); // Middle 6 bits.
			APPEND_CHAR(uint32_t(0x80 | (_replacement_char & 0x3f))); // Bottom 6 bits.
		}
	}
#undef APPEND_CHAR
	*cdst = 0; //trailing zero

	return utf8s;
}

String String::utf16(const char16_t *p_utf16, int p_len) {
	String ret;
	ret.parse_utf16(p_utf16, p_len, true);

	return ret;
}

Error String::parse_utf16(const char16_t *p_utf16, int p_len, bool p_default_little_endian) {
	if (!p_utf16) {
		return ERR_INVALID_DATA;
	}

	String aux;

	int cstr_size = 0;
	int str_size = 0;

#ifdef BIG_ENDIAN_ENABLED
	bool byteswap = p_default_little_endian;
#else
	bool byteswap = !p_default_little_endian;
#endif
	/* HANDLE BOM (Byte Order Mark) */
	if (p_len < 0 || p_len >= 1) {
		bool has_bom = false;
		if (uint16_t(p_utf16[0]) == 0xfeff) { // correct BOM, read as is
			has_bom = true;
			byteswap = false;
		} else if (uint16_t(p_utf16[0]) == 0xfffe) { // backwards BOM, swap bytes
			has_bom = true;
			byteswap = true;
		}
		if (has_bom) {
			if (p_len >= 0) {
				p_len -= 1;
			}
			p_utf16 += 1;
		}
	}

	bool decode_error = false;
	{
		const char16_t *ptrtmp = p_utf16;
		const char16_t *ptrtmp_limit = p_len >= 0 ? &p_utf16[p_len] : nullptr;
		uint32_t c_prev = 0;
		bool skip = false;
		while (ptrtmp != ptrtmp_limit && *ptrtmp) {
			uint32_t c = (byteswap) ? BSWAP16(*ptrtmp) : *ptrtmp;

			if ((c & 0xfffffc00) == 0xd800) { // lead surrogate
				if (skip) {
					print_unicode_error(vformat("Unpaired lead surrogate (%x [trail?] %x)", c_prev, c));
					decode_error = true;
				}
				skip = true;
			} else if ((c & 0xfffffc00) == 0xdc00) { // trail surrogate
				if (skip) {
					str_size--;
				} else {
					print_unicode_error(vformat("Unpaired trail surrogate (%x [lead?] %x)", c_prev, c));
					decode_error = true;
				}
				skip = false;
			} else {
				skip = false;
			}

			c_prev = c;
			str_size++;
			cstr_size++;
			ptrtmp++;
		}

		if (skip) {
			print_unicode_error(vformat("Unpaired lead surrogate (%x [eol])", c_prev));
			decode_error = true;
		}
	}

	if (str_size == 0) {
		clear();
		return OK; // empty string
	}

	resize(str_size + 1);
	char32_t *dst = ptrw();
	dst[str_size] = 0;

	bool skip = false;
	uint32_t c_prev = 0;
	while (cstr_size) {
		uint32_t c = (byteswap) ? BSWAP16(*p_utf16) : *p_utf16;

		if ((c & 0xfffffc00) == 0xd800) { // lead surrogate
			if (skip) {
				*(dst++) = c_prev; // unpaired, store as is
			}
			skip = true;
		} else if ((c & 0xfffffc00) == 0xdc00) { // trail surrogate
			if (skip) {
				*(dst++) = (c_prev << 10UL) + c - ((0xd800 << 10UL) + 0xdc00 - 0x10000); // decode pair
			} else {
				*(dst++) = c; // unpaired, store as is
			}
			skip = false;
		} else {
			*(dst++) = c;
			skip = false;
		}

		cstr_size--;
		p_utf16++;
		c_prev = c;
	}

	if (skip) {
		*(dst++) = c_prev;
	}

	if (decode_error) {
		return ERR_PARSE_ERROR;
	} else {
		return OK;
	}
}

Char16String String::utf16() const {
	int l = length();
	if (!l) {
		return Char16String();
	}

	const char32_t *d = &operator[](0);
	int fl = 0;
	for (int i = 0; i < l; i++) {
		uint32_t c = d[i];
		if (c <= 0xffff) { // 16 bits.
			fl += 1;
			if ((c & 0xfffff800) == 0xd800) {
				print_unicode_error(vformat("Unpaired surrogate (%x)", c));
			}
		} else if (c <= 0x10ffff) { // 32 bits.
			fl += 2;
		} else {
			print_unicode_error(vformat("Invalid unicode codepoint (%x), cannot represent as UTF-16", c), true);
			fl += 1;
		}
	}

	Char16String utf16s;
	if (fl == 0) {
		return utf16s;
	}

	utf16s.resize(fl + 1);
	uint16_t *cdst = (uint16_t *)utf16s.get_data();

#define APPEND_CHAR(m_c) *(cdst++) = m_c

	for (int i = 0; i < l; i++) {
		uint32_t c = d[i];

		if (c <= 0xffff) { // 16 bits.
			APPEND_CHAR(c);
		} else if (c <= 0x10ffff) { // 32 bits.
			APPEND_CHAR(uint32_t((c >> 10) + 0xd7c0)); // lead surrogate.
			APPEND_CHAR(uint32_t((c & 0x3ff) | 0xdc00)); // trail surrogate.
		} else {
			// the string is a valid UTF32, so it should never happen ...
			APPEND_CHAR(uint32_t((_replacement_char >> 10) + 0xd7c0));
			APPEND_CHAR(uint32_t((_replacement_char & 0x3ff) | 0xdc00));
		}
	}
#undef APPEND_CHAR
	*cdst = 0; //trailing zero

	return utf16s;
}

int64_t String::hex_to_int() const {
	int len = length();
	if (len == 0) {
		return 0;
	}

	const char32_t *s = ptr();

	int64_t sign = s[0] == '-' ? -1 : 1;

	if (sign < 0) {
		s++;
	}

	if (len > 2 && s[0] == '0' && lower_case(s[1]) == 'x') {
		s += 2;
	}

	int64_t hex = 0;

	while (*s) {
		char32_t c = lower_case(*s);
		int64_t n;
		if (is_digit(c)) {
			n = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			n = (c - 'a') + 10;
		} else {
			ERR_FAIL_V_MSG(0, vformat(R"(Invalid hexadecimal notation character "%c" (U+%04X) in string "%s".)", *s, static_cast<int32_t>(*s), *this));
		}
		// Check for overflow/underflow, with special case to ensure INT64_MIN does not result in error
		bool overflow = ((hex > INT64_MAX / 16) && (sign == 1 || (sign == -1 && hex != (INT64_MAX >> 4) + 1))) || (sign == -1 && hex == (INT64_MAX >> 4) + 1 && c > '0');
		ERR_FAIL_COND_V_MSG(overflow, sign == 1 ? INT64_MAX : INT64_MIN, "Cannot represent " + *this + " as a 64-bit signed integer, since the value is " + (sign == 1 ? "too large." : "too small."));
		hex *= 16;
		hex += n;
		s++;
	}

	return hex * sign;
}

int64_t String::bin_to_int() const {
	int len = length();
	if (len == 0) {
		return 0;
	}

	const char32_t *s = ptr();

	int64_t sign = s[0] == '-' ? -1 : 1;

	if (sign < 0) {
		s++;
	}

	if (len > 2 && s[0] == '0' && lower_case(s[1]) == 'b') {
		s += 2;
	}

	int64_t binary = 0;

	while (*s) {
		char32_t c = lower_case(*s);
		int64_t n;
		if (c == '0' || c == '1') {
			n = c - '0';
		} else {
			return 0;
		}
		// Check for overflow/underflow, with special case to ensure INT64_MIN does not result in error
		bool overflow = ((binary > INT64_MAX / 2) && (sign == 1 || (sign == -1 && binary != (INT64_MAX >> 1) + 1))) || (sign == -1 && binary == (INT64_MAX >> 1) + 1 && c > '0');
		ERR_FAIL_COND_V_MSG(overflow, sign == 1 ? INT64_MAX : INT64_MIN, "Cannot represent " + *this + " as a 64-bit signed integer, since the value is " + (sign == 1 ? "too large." : "too small."));
		binary *= 2;
		binary += n;
		s++;
	}

	return binary * sign;
}

template <typename C, typename T>
_ALWAYS_INLINE_ int64_t _to_int(const T &p_in, int to) {
	// Accumulate the total number in an unsigned integer as the range is:
	// +9223372036854775807 to -9223372036854775808 and the smallest negative
	// number does not fit inside an int64_t. So we accumulate the positive
	// number in an unsigned, and then at the very end convert to its signed
	// form.
	uint64_t integer = 0;
	uint8_t digits = 0;
	bool positive = true;

	for (int i = 0; i < to; i++) {
		C c = p_in[i];
		if (is_digit(c)) {
			// No need to do expensive checks unless we're approaching INT64_MAX / INT64_MIN.
			if (unlikely(digits > 18)) {
				bool overflow = (integer > INT64_MAX / 10) || (integer == INT64_MAX / 10 && ((positive && c > '7') || (!positive && c > '8')));
				ERR_FAIL_COND_V_MSG(overflow, positive ? INT64_MAX : INT64_MIN, "Cannot represent " + String(p_in) + " as a 64-bit signed integer, since the value is " + (positive ? "too large." : "too small."));
			}

			integer *= 10;
			integer += c - '0';
			++digits;

		} else if (integer == 0 && c == '-') {
			positive = !positive;
		}
	}

	if (positive) {
		return int64_t(integer);
	} else {
		return int64_t(integer * uint64_t(-1));
	}
}

int64_t String::to_int() const {
	if (length() == 0) {
		return 0;
	}

	int to = (find_char('.') >= 0) ? find_char('.') : length();

	return _to_int<char32_t>(*this, to);
}

int64_t String::to_int(const char *p_str, int p_len) {
	int to = 0;
	if (p_len >= 0) {
		to = p_len;
	} else {
		while (p_str[to] != 0 && p_str[to] != '.') {
			to++;
		}
	}

	return _to_int<char>(p_str, to);
}

int64_t String::to_int(const wchar_t *p_str, int p_len) {
	int to = 0;
	if (p_len >= 0) {
		to = p_len;
	} else {
		while (p_str[to] != 0 && p_str[to] != '.') {
			to++;
		}
	}

	return _to_int<wchar_t>(p_str, to);
}

bool String::is_numeric() const {
	if (length() == 0) {
		return false;
	}

	int s = 0;
	if (operator[](0) == '-') {
		++s;
	}
	bool dot = false;
	for (int i = s; i < length(); i++) {
		char32_t c = operator[](i);
		if (c == '.') {
			if (dot) {
				return false;
			}
			dot = true;
		} else if (!is_digit(c)) {
			return false;
		}
	}

	return true; // TODO: Use the parser below for this instead
}

template <typename C>
static double built_in_strtod(
		/* A decimal ASCII floating-point number,
		 * optionally preceded by white space. Must
		 * have form "-I.FE-X", where I is the integer
		 * part of the mantissa, F is the fractional
		 * part of the mantissa, and X is the
		 * exponent. Either of the signs may be "+",
		 * "-", or omitted. Either I or F may be
		 * omitted, or both. The decimal point isn't
		 * necessary unless F is present. The "E" may
		 * actually be an "e". E and X may both be
		 * omitted (but not just one). */
		const C *string,
		/* If non-nullptr, store terminating Cacter's
		 * address here. */
		C **endPtr = nullptr) {
	/* Largest possible base 10 exponent. Any
	 * exponent larger than this will already
	 * produce underflow or overflow, so there's
	 * no need to worry about additional digits. */
	static const int maxExponent = 511;
	/* Table giving binary powers of 10. Entry
	 * is 10^2^i. Used to convert decimal
	 * exponents into floating-point numbers. */
	static const double powersOf10[] = {
		10.,
		100.,
		1.0e4,
		1.0e8,
		1.0e16,
		1.0e32,
		1.0e64,
		1.0e128,
		1.0e256
	};

	bool sign, expSign = false;
	double fraction, dblExp;
	const double *d;
	const C *p;
	int c;
	/* Exponent read from "EX" field. */
	int exp = 0;
	/* Exponent that derives from the fractional
	 * part. Under normal circumstances, it is
	 * the negative of the number of digits in F.
	 * However, if I is very long, the last digits
	 * of I get dropped (otherwise a long I with a
	 * large negative exponent could cause an
	 * unnecessary overflow on I alone). In this
	 * case, fracExp is incremented one for each
	 * dropped digit. */
	int fracExp = 0;
	/* Number of digits in mantissa. */
	int mantSize;
	/* Number of mantissa digits BEFORE decimal point. */
	int decPt;
	/* Temporarily holds location of exponent in string. */
	const C *pExp;

	/*
	 * Strip off leading blanks and check for a sign.
	 */

	p = string;
	while (*p == ' ' || *p == '\t' || *p == '\n') {
		p += 1;
	}
	if (*p == '-') {
		sign = true;
		p += 1;
	} else {
		if (*p == '+') {
			p += 1;
		}
		sign = false;
	}

	/*
	 * Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point.
	 */

	decPt = -1;
	for (mantSize = 0;; mantSize += 1) {
		c = *p;
		if (!is_digit(c)) {
			if ((c != '.') || (decPt >= 0)) {
				break;
			}
			decPt = mantSize;
		}
		p += 1;
	}

	/*
	 * Now suck up the digits in the mantissa. Use two integers to collect 9
	 * digits each (this is faster than using floating-point). If the mantissa
	 * has more than 18 digits, ignore the extras, since they can't affect the
	 * value anyway.
	 */

	pExp = p;
	p -= mantSize;
	if (decPt < 0) {
		decPt = mantSize;
	} else {
		mantSize -= 1; /* One of the digits was the point. */
	}
	if (mantSize > 18) {
		fracExp = decPt - 18;
		mantSize = 18;
	} else {
		fracExp = decPt - mantSize;
	}
	if (mantSize == 0) {
		fraction = 0.0;
		p = string;
		goto done;
	} else {
		int frac1, frac2;

		frac1 = 0;
		for (; mantSize > 9; mantSize -= 1) {
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac1 = 10 * frac1 + (c - '0');
		}
		frac2 = 0;
		for (; mantSize > 0; mantSize -= 1) {
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac2 = 10 * frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/*
	 * Skim off the exponent.
	 */

	p = pExp;
	if ((*p == 'E') || (*p == 'e')) {
		p += 1;
		if (*p == '-') {
			expSign = true;
			p += 1;
		} else {
			if (*p == '+') {
				p += 1;
			}
			expSign = false;
		}
		if (!is_digit(char32_t(*p))) {
			p = pExp;
			goto done;
		}
		while (is_digit(char32_t(*p))) {
			exp = exp * 10 + (*p - '0');
			p += 1;
		}
	}
	if (expSign) {
		exp = fracExp - exp;
	} else {
		exp = fracExp + exp;
	}

	/*
	 * Generate a floating-point number that represents the exponent. Do this
	 * by processing the exponent one bit at a time to combine many powers of
	 * 2 of 10. Then combine the exponent with the fraction.
	 */

	if (exp < 0) {
		expSign = true;
		exp = -exp;
	} else {
		expSign = false;
	}

	if (exp > maxExponent) {
		exp = maxExponent;
		WARN_PRINT("Exponent too high");
	}
	dblExp = 1.0;
	for (d = powersOf10; exp != 0; exp >>= 1, ++d) {
		if (exp & 01) {
			dblExp *= *d;
		}
	}
	if (expSign) {
		fraction /= dblExp;
	} else {
		fraction *= dblExp;
	}

done:
	if (endPtr != nullptr) {
		*endPtr = (C *)p;
	}

	if (sign) {
		return -fraction;
	}
	return fraction;
}

#define READING_SIGN 0
#define READING_INT 1
#define READING_DEC 2
#define READING_EXP 3
#define READING_DONE 4

double String::to_float(const char *p_str) {
	return built_in_strtod<char>(p_str);
}

double String::to_float(const char32_t *p_str, const char32_t **r_end) {
	return built_in_strtod<char32_t>(p_str, (char32_t **)r_end);
}

double String::to_float(const wchar_t *p_str, const wchar_t **r_end) {
	return built_in_strtod<wchar_t>(p_str, (wchar_t **)r_end);
}

uint32_t String::num_characters(int64_t p_int) {
	int r = 1;
	if (p_int < 0) {
		r += 1;
		if (p_int == INT64_MIN) {
			p_int = INT64_MAX;
		} else {
			p_int = -p_int;
		}
	}
	while (p_int >= 10) {
		p_int /= 10;
		r++;
	}
	return r;
}

int64_t String::to_int(const char32_t *p_str, int p_len, bool p_clamp) {
	if (p_len == 0 || !p_str[0]) {
		return 0;
	}
	///@todo make more exact so saving and loading does not lose precision

	int64_t integer = 0;
	int64_t sign = 1;
	int reading = READING_SIGN;

	const char32_t *str = p_str;
	const char32_t *limit = &p_str[p_len];

	while (*str && reading != READING_DONE && str != limit) {
		char32_t c = *(str++);
		switch (reading) {
			case READING_SIGN: {
				if (is_digit(c)) {
					reading = READING_INT;
					// let it fallthrough
				} else if (c == '-') {
					sign = -1;
					reading = READING_INT;
					break;
				} else if (c == '+') {
					sign = 1;
					reading = READING_INT;
					break;
				} else {
					break;
				}
				[[fallthrough]];
			}
			case READING_INT: {
				if (is_digit(c)) {
					if (integer > INT64_MAX / 10) {
						String number("");
						str = p_str;
						while (*str && str != limit) {
							number += *(str++);
						}
						if (p_clamp) {
							if (sign == 1) {
								return INT64_MAX;
							} else {
								return INT64_MIN;
							}
						} else {
							ERR_FAIL_V_MSG(sign == 1 ? INT64_MAX : INT64_MIN, "Cannot represent " + number + " as a 64-bit signed integer, since the value is " + (sign == 1 ? "too large." : "too small."));
						}
					}
					integer *= 10;
					integer += c - '0';
				} else {
					reading = READING_DONE;
				}

			} break;
		}
	}

	return sign * integer;
}

double String::to_float() const {
	if (is_empty()) {
		return 0;
	}
	return built_in_strtod<char32_t>(get_data());
}

uint32_t String::hash(const char *p_cstr) {
	// static_cast: avoid negative values on platforms where char is signed.
	uint32_t hashv = 5381;
	uint32_t c = static_cast<uint8_t>(*p_cstr++);

	while (c) {
		hashv = ((hashv << 5) + hashv) + c; /* hash * 33 + c */
		c = static_cast<uint8_t>(*p_cstr++);
	}

	return hashv;
}

uint32_t String::hash(const char *p_cstr, int p_len) {
	uint32_t hashv = 5381;
	for (int i = 0; i < p_len; i++) {
		// static_cast: avoid negative values on platforms where char is signed.
		hashv = ((hashv << 5) + hashv) + static_cast<uint8_t>(p_cstr[i]); /* hash * 33 + c */
	}

	return hashv;
}

uint32_t String::hash(const wchar_t *p_cstr, int p_len) {
	// Avoid negative values on platforms where wchar_t is signed. Account for different sizes.
	using wide_unsigned = std::conditional<sizeof(wchar_t) == 2, uint16_t, uint32_t>::type;

	uint32_t hashv = 5381;
	for (int i = 0; i < p_len; i++) {
		hashv = ((hashv << 5) + hashv) + static_cast<wide_unsigned>(p_cstr[i]); /* hash * 33 + c */
	}

	return hashv;
}

uint32_t String::hash(const wchar_t *p_cstr) {
	// Avoid negative values on platforms where wchar_t is signed. Account for different sizes.
	using wide_unsigned = std::conditional<sizeof(wchar_t) == 2, uint16_t, uint32_t>::type;

	uint32_t hashv = 5381;
	uint32_t c = static_cast<wide_unsigned>(*p_cstr++);

	while (c) {
		hashv = ((hashv << 5) + hashv) + c; /* hash * 33 + c */
		c = static_cast<wide_unsigned>(*p_cstr++);
	}

	return hashv;
}

uint32_t String::hash(const char32_t *p_cstr, int p_len) {
	uint32_t hashv = 5381;
	for (int i = 0; i < p_len; i++) {
		hashv = ((hashv << 5) + hashv) + p_cstr[i]; /* hash * 33 + c */
	}

	return hashv;
}

uint32_t String::hash(const char32_t *p_cstr) {
	uint32_t hashv = 5381;
	uint32_t c = *p_cstr++;

	while (c) {
		hashv = ((hashv << 5) + hashv) + c; /* hash * 33 + c */
		c = *p_cstr++;
	}

	return hashv;
}

uint32_t String::hash() const {
	/* simple djb2 hashing */

	const char32_t *chr = get_data();
	uint32_t hashv = 5381;
	uint32_t c = *chr++;

	while (c) {
		hashv = ((hashv << 5) + hashv) + c; /* hash * 33 + c */
		c = *chr++;
	}

	return hashv;
}

uint64_t String::hash64() const {
	/* simple djb2 hashing */

	const char32_t *chr = get_data();
	uint64_t hashv = 5381;
	uint64_t c = *chr++;

	while (c) {
		hashv = ((hashv << 5) + hashv) + c; /* hash * 33 + c */
		c = *chr++;
	}

	return hashv;
}

String String::md5_text() const {
	CharString cs = utf8();
	unsigned char hash[16];
	CryptoCore::md5((unsigned char *)cs.ptr(), cs.length(), hash);
	return String::hex_encode_buffer(hash, 16);
}

String String::sha1_text() const {
	CharString cs = utf8();
	unsigned char hash[20];
	CryptoCore::sha1((unsigned char *)cs.ptr(), cs.length(), hash);
	return String::hex_encode_buffer(hash, 20);
}

String String::sha256_text() const {
	CharString cs = utf8();
	unsigned char hash[32];
	CryptoCore::sha256((unsigned char *)cs.ptr(), cs.length(), hash);
	return String::hex_encode_buffer(hash, 32);
}

Vector<uint8_t> String::md5_buffer() const {
	CharString cs = utf8();
	unsigned char hash[16];
	CryptoCore::md5((unsigned char *)cs.ptr(), cs.length(), hash);

	Vector<uint8_t> ret;
	ret.resize(16);
	uint8_t *ret_ptrw = ret.ptrw();
	for (int i = 0; i < 16; i++) {
		ret_ptrw[i] = hash[i];
	}
	return ret;
}

Vector<uint8_t> String::sha1_buffer() const {
	CharString cs = utf8();
	unsigned char hash[20];
	CryptoCore::sha1((unsigned char *)cs.ptr(), cs.length(), hash);

	Vector<uint8_t> ret;
	ret.resize(20);
	uint8_t *ret_ptrw = ret.ptrw();
	for (int i = 0; i < 20; i++) {
		ret_ptrw[i] = hash[i];
	}

	return ret;
}

Vector<uint8_t> String::sha256_buffer() const {
	CharString cs = utf8();
	unsigned char hash[32];
	CryptoCore::sha256((unsigned char *)cs.ptr(), cs.length(), hash);

	Vector<uint8_t> ret;
	ret.resize(32);
	uint8_t *ret_ptrw = ret.ptrw();
	for (int i = 0; i < 32; i++) {
		ret_ptrw[i] = hash[i];
	}
	return ret;
}

String String::insert(int p_at_pos, const String &p_string) const {
	if (p_string.is_empty() || p_at_pos < 0) {
		return *this;
	}

	if (p_at_pos > length()) {
		p_at_pos = length();
	}

	String ret;
	ret.resize(length() + p_string.length() + 1);
	char32_t *ret_ptrw = ret.ptrw();
	const char32_t *this_ptr = ptr();

	if (p_at_pos > 0) {
		memcpy(ret_ptrw, this_ptr, p_at_pos * sizeof(char32_t));
		ret_ptrw += p_at_pos;
	}

	memcpy(ret_ptrw, p_string.ptr(), p_string.length() * sizeof(char32_t));
	ret_ptrw += p_string.length();

	if (p_at_pos < length()) {
		memcpy(ret_ptrw, this_ptr + p_at_pos, (length() - p_at_pos) * sizeof(char32_t));
		ret_ptrw += length() - p_at_pos;
	}

	*ret_ptrw = 0;

	return ret;
}

String String::erase(int p_pos, int p_chars) const {
	ERR_FAIL_COND_V_MSG(p_pos < 0, "", vformat("Invalid starting position for `String.erase()`: %d. Starting position must be positive or zero.", p_pos));
	ERR_FAIL_COND_V_MSG(p_chars < 0, "", vformat("Invalid character count for `String.erase()`: %d. Character count must be positive or zero.", p_chars));
	return left(p_pos) + substr(p_pos + p_chars);
}

String String::substr(int p_from, int p_chars) const {
	if (p_chars == -1) {
		p_chars = length() - p_from;
	}

	if (is_empty() || p_from < 0 || p_from >= length() || p_chars <= 0) {
		return "";
	}

	if ((p_from + p_chars) > length()) {
		p_chars = length() - p_from;
	}

	if (p_from == 0 && p_chars >= length()) {
		return String(*this);
	}

	String s;
	s.copy_from_unchecked(&get_data()[p_from], p_chars);
	return s;
}

int String::find(const String &p_str, int p_from) const {
	if (p_from < 0) {
		return -1;
	}

	const int src_len = p_str.length();

	const int len = length();

	if (src_len == 0 || len == 0) {
		return -1; // won't find anything!
	}

	if (src_len == 1) {
		return find_char(p_str[0], p_from); // Optimize with single-char find.
	}

	const char32_t *src = get_data();
	const char32_t *str = p_str.get_data();

	for (int i = p_from; i <= (len - src_len); i++) {
		bool found = true;
		for (int j = 0; j < src_len; j++) {
			int read_pos = i + j;

			if (read_pos >= len) {
				ERR_PRINT("read_pos>=len");
				return -1;
			}

			if (src[read_pos] != str[j]) {
				found = false;
				break;
			}
		}

		if (found) {
			return i;
		}
	}

	return -1;
}

int String::find(const char *p_str, int p_from) const {
	if (p_from < 0 || !p_str) {
		return -1;
	}

	const int src_len = strlen(p_str);

	const int len = length();

	if (len == 0 || src_len == 0) {
		return -1; // won't find anything!
	}

	if (src_len == 1) {
		return find_char(*p_str, p_from); // Optimize with single-char find.
	}

	const char32_t *src = get_data();

	if (src_len == 1) {
		const char32_t needle = p_str[0];

		for (int i = p_from; i < len; i++) {
			if (src[i] == needle) {
				return i;
			}
		}

	} else {
		for (int i = p_from; i <= (len - src_len); i++) {
			bool found = true;
			for (int j = 0; j < src_len; j++) {
				int read_pos = i + j;

				if (read_pos >= len) {
					ERR_PRINT("read_pos>=len");
					return -1;
				}

				if (src[read_pos] != (char32_t)p_str[j]) {
					found = false;
					break;
				}
			}

			if (found) {
				return i;
			}
		}
	}

	return -1;
}

int String::find_char(char32_t p_char, int p_from) const {
	return _cowdata.find(p_char, p_from);
}

int String::findmk(const Vector<String> &p_keys, int p_from, int *r_key) const {
	if (p_from < 0) {
		return -1;
	}
	if (p_keys.size() == 0) {
		return -1;
	}

	//int src_len=p_str.length();
	const String *keys = &p_keys[0];
	int key_count = p_keys.size();
	int len = length();

	if (len == 0) {
		return -1; // won't find anything!
	}

	const char32_t *src = get_data();

	for (int i = p_from; i < len; i++) {
		bool found = true;
		for (int k = 0; k < key_count; k++) {
			found = true;
			if (r_key) {
				*r_key = k;
			}
			const char32_t *cmp = keys[k].get_data();
			int l = keys[k].length();

			for (int j = 0; j < l; j++) {
				int read_pos = i + j;

				if (read_pos >= len) {
					found = false;
					break;
				}

				if (src[read_pos] != cmp[j]) {
					found = false;
					break;
				}
			}
			if (found) {
				break;
			}
		}

		if (found) {
			return i;
		}
	}

	return -1;
}

int String::findn(const String &p_str, int p_from) const {
	if (p_from < 0) {
		return -1;
	}

	int src_len = p_str.length();

	if (src_len == 0 || length() == 0) {
		return -1; // won't find anything!
	}

	const char32_t *srcd = get_data();

	for (int i = p_from; i <= (length() - src_len); i++) {
		bool found = true;
		for (int j = 0; j < src_len; j++) {
			int read_pos = i + j;

			if (read_pos >= length()) {
				ERR_PRINT("read_pos>=length()");
				return -1;
			}

			char32_t src = _find_lower(srcd[read_pos]);
			char32_t dst = _find_lower(p_str[j]);

			if (src != dst) {
				found = false;
				break;
			}
		}

		if (found) {
			return i;
		}
	}

	return -1;
}

int String::findn(const char *p_str, int p_from) const {
	if (p_from < 0) {
		return -1;
	}

	int src_len = strlen(p_str);

	if (src_len == 0 || length() == 0) {
		return -1; // won't find anything!
	}

	const char32_t *srcd = get_data();

	for (int i = p_from; i <= (length() - src_len); i++) {
		bool found = true;
		for (int j = 0; j < src_len; j++) {
			int read_pos = i + j;

			if (read_pos >= length()) {
				ERR_PRINT("read_pos>=length()");
				return -1;
			}

			char32_t src = _find_lower(srcd[read_pos]);
			char32_t dst = _find_lower(p_str[j]);

			if (src != dst) {
				found = false;
				break;
			}
		}

		if (found) {
			return i;
		}
	}

	return -1;
}

int String::rfind(const String &p_str, int p_from) const {
	// establish a limit
	int limit = length() - p_str.length();
	if (limit < 0) {
		return -1;
	}

	// establish a starting point
	if (p_from < 0) {
		p_from = limit;
	} else if (p_from > limit) {
		p_from = limit;
	}

	int src_len = p_str.length();
	int len = length();

	if (src_len == 0 || len == 0) {
		return -1; // won't find anything!
	}

	const char32_t *src = get_data();

	for (int i = p_from; i >= 0; i--) {
		bool found = true;
		for (int j = 0; j < src_len; j++) {
			int read_pos = i + j;

			if (read_pos >= len) {
				ERR_PRINT("read_pos>=len");
				return -1;
			}

			if (src[read_pos] != p_str[j]) {
				found = false;
				break;
			}
		}

		if (found) {
			return i;
		}
	}

	return -1;
}

int String::rfind(const char *p_str, int p_from) const {
	const int source_length = length();
	int substring_length = strlen(p_str);

	if (source_length == 0 || substring_length == 0) {
		return -1; // won't find anything!
	}

	// establish a limit
	int limit = length() - substring_length;
	if (limit < 0) {
		return -1;
	}

	// establish a starting point
	int starting_point;
	if (p_from < 0) {
		starting_point = limit;
	} else if (p_from > limit) {
		starting_point = limit;
	} else {
		starting_point = p_from;
	}

	const char32_t *source = get_data();

	for (int i = starting_point; i >= 0; i--) {
		bool found = true;
		for (int j = 0; j < substring_length; j++) {
			int read_pos = i + j;

			if (read_pos >= source_length) {
				ERR_PRINT("read_pos>=source_length");
				return -1;
			}

			const char32_t key_needle = p_str[j];
			if (source[read_pos] != key_needle) {
				found = false;
				break;
			}
		}

		if (found) {
			return i;
		}
	}

	return -1;
}

int String::rfind_char(char32_t p_char, int p_from) const {
	return _cowdata.rfind(p_char, p_from);
}

int String::rfindn(const String &p_str, int p_from) const {
	// establish a limit
	int limit = length() - p_str.length();
	if (limit < 0) {
		return -1;
	}

	// establish a starting point
	if (p_from < 0) {
		p_from = limit;
	} else if (p_from > limit) {
		p_from = limit;
	}

	int src_len = p_str.length();
	int len = length();

	if (src_len == 0 || len == 0) {
		return -1; // won't find anything!
	}

	const char32_t *src = get_data();

	for (int i = p_from; i >= 0; i--) {
		bool found = true;
		for (int j = 0; j < src_len; j++) {
			int read_pos = i + j;

			if (read_pos >= len) {
				ERR_PRINT("read_pos>=len");
				return -1;
			}

			char32_t srcc = _find_lower(src[read_pos]);
			char32_t dstc = _find_lower(p_str[j]);

			if (srcc != dstc) {
				found = false;
				break;
			}
		}

		if (found) {
			return i;
		}
	}

	return -1;
}

int String::rfindn(const char *p_str, int p_from) const {
	const int source_length = length();
	int substring_length = strlen(p_str);

	if (source_length == 0 || substring_length == 0) {
		return -1; // won't find anything!
	}

	// establish a limit
	int limit = length() - substring_length;
	if (limit < 0) {
		return -1;
	}

	// establish a starting point
	int starting_point;
	if (p_from < 0) {
		starting_point = limit;
	} else if (p_from > limit) {
		starting_point = limit;
	} else {
		starting_point = p_from;
	}

	const char32_t *source = get_data();

	for (int i = starting_point; i >= 0; i--) {
		bool found = true;
		for (int j = 0; j < substring_length; j++) {
			int read_pos = i + j;

			if (read_pos >= source_length) {
				ERR_PRINT("read_pos>=source_length");
				return -1;
			}

			const char32_t key_needle = p_str[j];
			int srcc = _find_lower(source[read_pos]);
			int keyc = _find_lower(key_needle);

			if (srcc != keyc) {
				found = false;
				break;
			}
		}

		if (found) {
			return i;
		}
	}

	return -1;
}

bool String::ends_with(const String &p_string) const {
	const int l = p_string.length();
	if (l > length()) {
		return false;
	}
	if (l == 0) {
		return true;
	}

	return memcmp(ptr() + (length() - l), p_string.ptr(), l * sizeof(char32_t)) == 0;
}

bool String::ends_with(const char *p_string) const {
	if (!p_string) {
		return false;
	}

	int l = strlen(p_string);
	if (l > length()) {
		return false;
	}

	if (l == 0) {
		return true;
	}

	const char32_t *s = &operator[](length() - l);

	for (int i = 0; i < l; i++) {
		if (static_cast<char32_t>(p_string[i]) != s[i]) {
			return false;
		}
	}

	return true;
}

bool String::begins_with(const String &p_string) const {
	const int l = p_string.length();
	if (l > length()) {
		return false;
	}
	if (l == 0) {
		return true;
	}

	return memcmp(ptr(), p_string.ptr(), l * sizeof(char32_t)) == 0;
}

bool String::begins_with(const char *p_string) const {
	if (!p_string) {
		return false;
	}

	int l = length();
	if (l == 0) {
		return *p_string == 0;
	}

	const char32_t *str = &operator[](0);
	int i = 0;

	while (*p_string && i < l) {
		if ((char32_t)*p_string != str[i]) {
			return false;
		}
		i++;
		p_string++;
	}

	return *p_string == 0;
}

bool String::is_enclosed_in(const String &p_string) const {
	return begins_with(p_string) && ends_with(p_string);
}

bool String::is_subsequence_of(const String &p_string) const {
	return _base_is_subsequence_of(p_string, false);
}

bool String::is_subsequence_ofn(const String &p_string) const {
	return _base_is_subsequence_of(p_string, true);
}

bool String::is_quoted() const {
	return is_enclosed_in("\"") || is_enclosed_in("'");
}

bool String::is_lowercase() const {
	for (const char32_t *str = &operator[](0); *str; str++) {
		if (is_unicode_upper_case(*str)) {
			return false;
		}
	}
	return true;
}

int String::_count(const String &p_string, int p_from, int p_to, bool p_case_insensitive) const {
	if (p_string.is_empty()) {
		return 0;
	}
	int len = length();
	int slen = p_string.length();
	if (len < slen) {
		return 0;
	}
	String str;
	if (p_from >= 0 && p_to >= 0) {
		if (p_to == 0) {
			p_to = len;
		} else if (p_from >= p_to) {
			return 0;
		}
		if (p_from == 0 && p_to == len) {
			str = *this;
		} else {
			str = substr(p_from, p_to - p_from);
		}
	} else {
		return 0;
	}
	int c = 0;
	int idx = 0;
	while ((idx = p_case_insensitive ? str.findn(p_string, idx) : str.find(p_string, idx)) != -1) {
		// Skip the occurrence itself.
		idx += slen;
		++c;
	}
	return c;
}

int String::_count(const char *p_string, int p_from, int p_to, bool p_case_insensitive) const {
	int substring_length = strlen(p_string);
	if (substring_length == 0) {
		return 0;
	}
	const int source_length = length();

	if (source_length < substring_length) {
		return 0;
	}
	String str;
	int search_limit = p_to;
	if (p_from >= 0 && p_to >= 0) {
		if (p_to == 0) {
			search_limit = source_length;
		} else if (p_from >= p_to) {
			return 0;
		}
		if (p_from == 0 && search_limit == source_length) {
			str = *this;
		} else {
			str = substr(p_from, search_limit - p_from);
		}
	} else {
		return 0;
	}
	int c = 0;
	int idx = 0;
	while ((idx = p_case_insensitive ? str.findn(p_string, idx) : str.find(p_string, idx)) != -1) {
		// Skip the occurrence itself.
		idx += substring_length;
		++c;
	}
	return c;
}

int String::count(const String &p_string, int p_from, int p_to) const {
	return _count(p_string, p_from, p_to, false);
}

int String::count(const char *p_string, int p_from, int p_to) const {
	return _count(p_string, p_from, p_to, false);
}

int String::countn(const String &p_string, int p_from, int p_to) const {
	return _count(p_string, p_from, p_to, true);
}

int String::countn(const char *p_string, int p_from, int p_to) const {
	return _count(p_string, p_from, p_to, true);
}

bool String::_base_is_subsequence_of(const String &p_string, bool case_insensitive) const {
	int len = length();
	if (len == 0) {
		// Technically an empty string is subsequence of any string
		return true;
	}

	if (len > p_string.length()) {
		return false;
	}

	const char32_t *src = &operator[](0);
	const char32_t *tgt = &p_string[0];

	for (; *src && *tgt; tgt++) {
		bool match = false;
		if (case_insensitive) {
			char32_t srcc = _find_lower(*src);
			char32_t tgtc = _find_lower(*tgt);
			match = srcc == tgtc;
		} else {
			match = *src == *tgt;
		}
		if (match) {
			src++;
			if (!*src) {
				return true;
			}
		}
	}

	return false;
}

Vector<String> String::bigrams() const {
	int n_pairs = length() - 1;
	Vector<String> b;
	if (n_pairs <= 0) {
		return b;
	}
	b.resize(n_pairs);
	String *b_ptrw = b.ptrw();
	for (int i = 0; i < n_pairs; i++) {
		b_ptrw[i] = substr(i, 2);
	}
	return b;
}

// Similarity according to Sorensen-Dice coefficient
float String::similarity(const String &p_string) const {
	if (operator==(p_string)) {
		// Equal strings are totally similar
		return 1.0f;
	}
	if (length() < 2 || p_string.length() < 2) {
		// No way to calculate similarity without a single bigram
		return 0.0f;
	}

	const int src_size = length() - 1;
	const int tgt_size = p_string.length() - 1;

	const int sum = src_size + tgt_size;
	int inter = 0;
	for (int i = 0; i < src_size; i++) {
		const char32_t i0 = get(i);
		const char32_t i1 = get(i + 1);

		for (int j = 0; j < tgt_size; j++) {
			if (i0 == p_string.get(j) && i1 == p_string.get(j + 1)) {
				inter++;
				break;
			}
		}
	}

	return (2.0f * inter) / sum;
}

static bool _wildcard_match(const char32_t *p_pattern, const char32_t *p_string, bool p_case_sensitive) {
	switch (*p_pattern) {
		case '\0':
			return !*p_string;
		case '*':
			return _wildcard_match(p_pattern + 1, p_string, p_case_sensitive) || (*p_string && _wildcard_match(p_pattern, p_string + 1, p_case_sensitive));
		case '?':
			return *p_string && (*p_string != '.') && _wildcard_match(p_pattern + 1, p_string + 1, p_case_sensitive);
		default:

			return (p_case_sensitive ? (*p_string == *p_pattern) : (_find_upper(*p_string) == _find_upper(*p_pattern))) && _wildcard_match(p_pattern + 1, p_string + 1, p_case_sensitive);
	}
}

bool String::match(const String &p_wildcard) const {
	if (!p_wildcard.length() || !length()) {
		return false;
	}

	return _wildcard_match(p_wildcard.get_data(), get_data(), true);
}

bool String::matchn(const String &p_wildcard) const {
	if (!p_wildcard.length() || !length()) {
		return false;
	}
	return _wildcard_match(p_wildcard.get_data(), get_data(), false);
}

String String::format(const Variant &values, const String &placeholder) const {
	String new_string = String(ptr());

	if (values.get_type() == Variant::ARRAY) {
		Array values_arr = values;

		for (int i = 0; i < values_arr.size(); i++) {
			String i_as_str = String::num_int64(i);

			if (values_arr[i].get_type() == Variant::ARRAY) { //Array in Array structure [["name","RobotGuy"],[0,"godot"],["strength",9000.91]]
				Array value_arr = values_arr[i];

				if (value_arr.size() == 2) {
					Variant v_key = value_arr[0];
					String key = v_key;

					Variant v_val = value_arr[1];
					String val = v_val;

					new_string = new_string.replace(placeholder.replace("_", key), val);
				} else {
					ERR_PRINT(String("STRING.format Inner Array size != 2 ").ascii().get_data());
				}
			} else { //Array structure ["RobotGuy","Logis","rookie"]
				Variant v_val = values_arr[i];
				String val = v_val;

				if (placeholder.contains_char('_')) {
					new_string = new_string.replace(placeholder.replace("_", i_as_str), val);
				} else {
					new_string = new_string.replace_first(placeholder, val);
				}
			}
		}
	} else if (values.get_type() == Variant::DICTIONARY) {
		Dictionary d = values;
		List<Variant> keys;
		d.get_key_list(&keys);

		for (const Variant &key : keys) {
			new_string = new_string.replace(placeholder.replace("_", key), d[key]);
		}
	} else if (values.get_type() == Variant::OBJECT) {
		Object *obj = values.get_validated_object();
		ERR_FAIL_NULL_V(obj, new_string);

		List<PropertyInfo> props;
		obj->get_property_list(&props);

		for (const PropertyInfo &E : props) {
			new_string = new_string.replace(placeholder.replace("_", E.name), obj->get(E.name));
		}
	} else {
		ERR_PRINT(String("Invalid type: use Array, Dictionary or Object.").ascii().get_data());
	}

	return new_string;
}

static String _replace_common(const String &p_this, const String &p_key, const String &p_with, bool p_case_insensitive) {
	if (p_key.is_empty() || p_this.is_empty()) {
		return p_this;
	}

	const size_t key_length = p_key.length();

	int search_from = 0;
	int result = 0;

	LocalVector<int> found;

	while ((result = (p_case_insensitive ? p_this.findn(p_key, search_from) : p_this.find(p_key, search_from))) >= 0) {
		found.push_back(result);
		ERR_FAIL_COND_V_MSG((result + key_length) > INT32_MAX, p_this, "Key length too long");
		search_from = result + key_length;
	}

	if (found.is_empty()) {
		return p_this;
	}

	String new_string;

	const int with_length = p_with.length();
	const int old_length = p_this.length();

	new_string.resize(old_length + int(found.size()) * (with_length - key_length) + 1);

	char32_t *new_ptrw = new_string.ptrw();
	const char32_t *old_ptr = p_this.ptr();
	const char32_t *with_ptr = p_with.ptr();

	int last_pos = 0;

	for (const int &pos : found) {
		if (last_pos != pos) {
			memcpy(new_ptrw, old_ptr + last_pos, (pos - last_pos) * sizeof(char32_t));
			new_ptrw += (pos - last_pos);
		}
		if (with_length) {
			memcpy(new_ptrw, with_ptr, with_length * sizeof(char32_t));
			new_ptrw += with_length;
		}
		last_pos = pos + key_length;
	}

	if (last_pos != old_length) {
		memcpy(new_ptrw, old_ptr + last_pos, (old_length - last_pos) * sizeof(char32_t));
		new_ptrw += old_length - last_pos;
	}

	*new_ptrw = 0;

	return new_string;
}

static String _replace_common(const String &p_this, char const *p_key, char const *p_with, bool p_case_insensitive) {
	size_t key_length = strlen(p_key);

	if (key_length == 0 || p_this.is_empty()) {
		return p_this;
	}

	int search_from = 0;
	int result = 0;

	LocalVector<int> found;

	while ((result = (p_case_insensitive ? p_this.findn(p_key, search_from) : p_this.find(p_key, search_from))) >= 0) {
		found.push_back(result);
		ERR_FAIL_COND_V_MSG((result + key_length) > INT32_MAX, p_this, "Key length too long");
		search_from = result + key_length;
	}

	if (found.is_empty()) {
		return p_this;
	}

	String new_string;

	// Create string to speed up copying as we can't do `memcopy` between `char32_t` and `char`.
	const String with_string(p_with);
	const int with_length = with_string.length();
	const int old_length = p_this.length();

	new_string.resize(old_length + int(found.size()) * (with_length - key_length) + 1);

	char32_t *new_ptrw = new_string.ptrw();
	const char32_t *old_ptr = p_this.ptr();
	const char32_t *with_ptr = with_string.ptr();

	int last_pos = 0;

	for (const int &pos : found) {
		if (last_pos != pos) {
			memcpy(new_ptrw, old_ptr + last_pos, (pos - last_pos) * sizeof(char32_t));
			new_ptrw += (pos - last_pos);
		}
		if (with_length) {
			memcpy(new_ptrw, with_ptr, with_length * sizeof(char32_t));
			new_ptrw += with_length;
		}
		last_pos = pos + key_length;
	}

	if (last_pos != old_length) {
		memcpy(new_ptrw, old_ptr + last_pos, (old_length - last_pos) * sizeof(char32_t));
		new_ptrw += old_length - last_pos;
	}

	*new_ptrw = 0;

	return new_string;
}

String String::replace(const String &p_key, const String &p_with) const {
	return _replace_common(*this, p_key, p_with, false);
}

String String::replace(const char *p_key, const char *p_with) const {
	return _replace_common(*this, p_key, p_with, false);
}

String String::replace_first(const String &p_key, const String &p_with) const {
	int pos = find(p_key);
	if (pos >= 0) {
		const int old_length = length();
		const int key_length = p_key.length();
		const int with_length = p_with.length();

		String new_string;
		new_string.resize(old_length + (with_length - key_length) + 1);

		char32_t *new_ptrw = new_string.ptrw();
		const char32_t *old_ptr = ptr();
		const char32_t *with_ptr = p_with.ptr();

		if (pos > 0) {
			memcpy(new_ptrw, old_ptr, pos * sizeof(char32_t));
			new_ptrw += pos;
		}

		if (with_length) {
			memcpy(new_ptrw, with_ptr, with_length * sizeof(char32_t));
			new_ptrw += with_length;
		}
		pos += key_length;

		if (pos != old_length) {
			memcpy(new_ptrw, old_ptr + pos, (old_length - pos) * sizeof(char32_t));
			new_ptrw += (old_length - pos);
		}

		*new_ptrw = 0;

		return new_string;
	}

	return *this;
}

String String::replace_first(const char *p_key, const char *p_with) const {
	int pos = find(p_key);
	if (pos >= 0) {
		const int old_length = length();
		const int key_length = strlen(p_key);
		const int with_length = strlen(p_with);

		String new_string;
		new_string.resize(old_length + (with_length - key_length) + 1);

		char32_t *new_ptrw = new_string.ptrw();
		const char32_t *old_ptr = ptr();

		if (pos > 0) {
			memcpy(new_ptrw, old_ptr, pos * sizeof(char32_t));
			new_ptrw += pos;
		}

		for (int i = 0; i < with_length; ++i) {
			*new_ptrw++ = p_with[i];
		}
		pos += key_length;

		if (pos != old_length) {
			memcpy(new_ptrw, old_ptr + pos, (old_length - pos) * sizeof(char32_t));
			new_ptrw += (old_length - pos);
		}

		*new_ptrw = 0;

		return new_string;
	}

	return *this;
}

String String::replacen(const String &p_key, const String &p_with) const {
	return _replace_common(*this, p_key, p_with, true);
}

String String::replacen(const char *p_key, const char *p_with) const {
	return _replace_common(*this, p_key, p_with, true);
}

String String::repeat(int p_count) const {
	ERR_FAIL_COND_V_MSG(p_count < 0, "", "Parameter count should be a positive number.");

	if (p_count == 0) {
		return "";
	}

	if (p_count == 1) {
		return *this;
	}

	int len = length();
	String new_string = *this;
	new_string.resize(p_count * len + 1);

	char32_t *dst = new_string.ptrw();
	int offset = 1;
	int stride = 1;
	while (offset < p_count) {
		memcpy(dst + offset * len, dst, stride * len * sizeof(char32_t));
		offset += stride;
		stride = MIN(stride * 2, p_count - offset);
	}
	dst[p_count * len] = _null;
	return new_string;
}

String String::reverse() const {
	int len = length();
	if (len <= 1) {
		return *this;
	}
	String new_string;
	new_string.resize(len + 1);

	const char32_t *src = ptr();
	char32_t *dst = new_string.ptrw();
	for (int i = 0; i < len; i++) {
		dst[i] = src[len - i - 1];
	}
	dst[len] = _null;
	return new_string;
}

String String::left(int p_len) const {
	if (p_len < 0) {
		p_len = length() + p_len;
	}

	if (p_len <= 0) {
		return "";
	}

	if (p_len >= length()) {
		return *this;
	}

	String s;
	s.copy_from_unchecked(&get_data()[0], p_len);
	return s;
}

String String::right(int p_len) const {
	if (p_len < 0) {
		p_len = length() + p_len;
	}

	if (p_len <= 0) {
		return "";
	}

	if (p_len >= length()) {
		return *this;
	}

	String s;
	s.copy_from_unchecked(&get_data()[length() - p_len], p_len);
	return s;
}

char32_t String::unicode_at(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, length(), 0);
	return operator[](p_idx);
}

String String::indent(const String &p_prefix) const {
	String new_string;
	int line_start = 0;

	for (int i = 0; i < length(); i++) {
		const char32_t c = operator[](i);
		if (c == '\n') {
			if (i == line_start) {
				new_string += c; // Leave empty lines empty.
			} else {
				new_string += p_prefix + substr(line_start, i - line_start + 1);
			}
			line_start = i + 1;
		}
	}
	if (line_start != length()) {
		new_string += p_prefix + substr(line_start);
	}
	return new_string;
}

String String::dedent() const {
	String new_string;
	String indent;
	bool has_indent = false;
	bool has_text = false;
	int line_start = 0;
	int indent_stop = -1;

	for (int i = 0; i < length(); i++) {
		char32_t c = operator[](i);
		if (c == '\n') {
			if (has_text) {
				new_string += substr(indent_stop, i - indent_stop);
			}
			new_string += "\n";
			has_text = false;
			line_start = i + 1;
			indent_stop = -1;
		} else if (!has_text) {
			if (c > 32) {
				has_text = true;
				if (!has_indent) {
					has_indent = true;
					indent = substr(line_start, i - line_start);
					indent_stop = i;
				}
			}
			if (has_indent && indent_stop < 0) {
				int j = i - line_start;
				if (j >= indent.length() || c != indent[j]) {
					indent_stop = i;
				}
			}
		}
	}

	if (has_text) {
		new_string += substr(indent_stop, length() - indent_stop);
	}

	return new_string;
}

String String::strip_edges(bool left, bool right) const {
	int len = length();
	int beg = 0, end = len;

	if (left) {
		for (int i = 0; i < len; i++) {
			if (operator[](i) <= 32) {
				beg++;
			} else {
				break;
			}
		}
	}

	if (right) {
		for (int i = len - 1; i >= 0; i--) {
			if (operator[](i) <= 32) {
				end--;
			} else {
				break;
			}
		}
	}

	if (beg == 0 && end == len) {
		return *this;
	}

	return substr(beg, end - beg);
}

String String::strip_escapes() const {
	String new_string;
	for (int i = 0; i < length(); i++) {
		// Escape characters on first page of the ASCII table, before 32 (Space).
		if (operator[](i) < 32) {
			continue;
		}
		new_string += operator[](i);
	}

	return new_string;
}

String String::lstrip(const String &p_chars) const {
	int len = length();
	int beg;

	for (beg = 0; beg < len; beg++) {
		if (p_chars.find_char(get(beg)) == -1) {
			break;
		}
	}

	if (beg == 0) {
		return *this;
	}

	return substr(beg, len - beg);
}

String String::rstrip(const String &p_chars) const {
	int len = length();
	int end;

	for (end = len - 1; end >= 0; end--) {
		if (p_chars.find_char(get(end)) == -1) {
			break;
		}
	}

	if (end == len - 1) {
		return *this;
	}

	return substr(0, end + 1);
}

bool String::is_network_share_path() const {
	return begins_with("//") || begins_with("\\\\");
}

String String::simplify_path() const {
	String s = *this;
	String drive;

	// Check if we have a special path (like res://) or a protocol identifier.
	int p = s.find("://");
	bool found = false;
	if (p > 0) {
		bool only_chars = true;
		for (int i = 0; i < p; i++) {
			if (!is_ascii_alphanumeric_char(s[i])) {
				only_chars = false;
				break;
			}
		}
		if (only_chars) {
			found = true;
			drive = s.substr(0, p + 3);
			s = s.substr(p + 3);
		}
	}
	if (!found) {
		if (is_network_share_path()) {
			// Network path, beginning with // or \\.
			drive = s.substr(0, 2);
			s = s.substr(2);
		} else if (s.begins_with("/") || s.begins_with("\\")) {
			// Absolute path.
			drive = s.substr(0, 1);
			s = s.substr(1);
		} else {
			// Windows-style drive path, like C:/ or C:\.
			p = s.find(":/");
			if (p == -1) {
				p = s.find(":\\");
			}
			if (p != -1 && p < s.find_char('/')) {
				drive = s.substr(0, p + 2);
				s = s.substr(p + 2);
			}
		}
	}

	s = s.replace("\\", "/");
	while (true) { // in case of using 2 or more slash
		String compare = s.replace("//", "/");
		if (s == compare) {
			break;
		} else {
			s = compare;
		}
	}
	Vector<String> dirs = s.split("/", false);

	for (int i = 0; i < dirs.size(); i++) {
		String d = dirs[i];
		if (d == ".") {
			dirs.remove_at(i);
			i--;
		} else if (d == "..") {
			if (i != 0 && dirs[i - 1] != "..") {
				dirs.remove_at(i);
				dirs.remove_at(i - 1);
				i -= 2;
			}
		}
	}

	s = "";

	for (int i = 0; i < dirs.size(); i++) {
		if (i > 0) {
			s += "/";
		}
		s += dirs[i];
	}

	return drive + s;
}

static int _humanize_digits(int p_num) {
	if (p_num < 100) {
		return 2;
	} else if (p_num < 1024) {
		return 1;
	} else {
		return 0;
	}
}

String String::humanize_size(uint64_t p_size) {
	int magnitude = 0;
	uint64_t _div = 1;
	while (p_size > _div * 1024 && magnitude < 6) {
		_div *= 1024;
		magnitude++;
	}

	if (magnitude == 0) {
		return String::num_uint64(p_size) + " " + RTR("B");
	} else {
		String suffix;
		switch (magnitude) {
			case 1:
				suffix = RTR("KiB");
				break;
			case 2:
				suffix = RTR("MiB");
				break;
			case 3:
				suffix = RTR("GiB");
				break;
			case 4:
				suffix = RTR("TiB");
				break;
			case 5:
				suffix = RTR("PiB");
				break;
			case 6:
				suffix = RTR("EiB");
				break;
		}

		const double divisor = _div;
		const int digits = _humanize_digits(p_size / _div);
		return String::num(p_size / divisor).pad_decimals(digits) + " " + suffix;
	}
}

bool String::is_absolute_path() const {
	if (length() > 1) {
		return (operator[](0) == '/' || operator[](0) == '\\' || find(":/") != -1 || find(":\\") != -1);
	} else if ((length()) == 1) {
		return (operator[](0) == '/' || operator[](0) == '\\');
	} else {
		return false;
	}
}

String String::validate_ascii_identifier() const {
	if (is_empty()) {
		return "_"; // Empty string is not a valid identifier.
	}

	String result;
	if (is_digit(operator[](0))) {
		result = "_" + *this;
	} else {
		result = *this;
	}

	int len = result.length();
	char32_t *buffer = result.ptrw();
	for (int i = 0; i < len; i++) {
		if (!is_ascii_identifier_char(buffer[i])) {
			buffer[i] = '_';
		}
	}

	return result;
}

String String::validate_unicode_identifier() const {
	if (is_empty()) {
		return "_"; // Empty string is not a valid identifier.
	}

	String result;
	if (is_unicode_identifier_start(operator[](0))) {
		result = *this;
	} else {
		result = "_" + *this;
	}

	int len = result.length();
	char32_t *buffer = result.ptrw();
	for (int i = 0; i < len; i++) {
		if (!is_unicode_identifier_continue(buffer[i])) {
			buffer[i] = '_';
		}
	}

	return result;
}

bool String::is_valid_ascii_identifier() const {
	int len = length();

	if (len == 0) {
		return false;
	}

	if (is_digit(operator[](0))) {
		return false;
	}

	const char32_t *str = &operator[](0);

	for (int i = 0; i < len; i++) {
		if (!is_ascii_identifier_char(str[i])) {
			return false;
		}
	}

	return true;
}

bool String::is_valid_unicode_identifier() const {
	const char32_t *str = ptr();
	int len = length();

	if (len == 0) {
		return false; // Empty string.
	}

	if (!is_unicode_identifier_start(str[0])) {
		return false;
	}

	for (int i = 1; i < len; i++) {
		if (!is_unicode_identifier_continue(str[i])) {
			return false;
		}
	}
	return true;
}

bool String::is_valid_string() const {
	int l = length();
	const char32_t *src = get_data();
	bool valid = true;
	for (int i = 0; i < l; i++) {
		valid = valid && (src[i] < 0xd800 || (src[i] > 0xdfff && src[i] <= 0x10ffff));
	}
	return valid;
}

String String::uri_encode() const {
	const CharString temp = utf8();
	String res;

	for (int i = 0; i < temp.length(); ++i) {
		uint8_t ord = uint8_t(temp[i]);
		if (ord == '.' || ord == '-' || ord == '~' || is_ascii_identifier_char(ord)) {
			res += ord;
		} else {
			char p[4] = { '%', 0, 0, 0 };
			static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
			p[1] = hex[ord >> 4];
			p[2] = hex[ord & 0xF];
			res += p;
		}
	}
	return res;
}

String String::uri_decode() const {
	CharString src = utf8();
	CharString res;
	for (int i = 0; i < src.length(); ++i) {
		if (src[i] == '%' && i + 2 < src.length()) {
			char ord1 = src[i + 1];
			if (is_digit(ord1) || is_ascii_upper_case(ord1)) {
				char ord2 = src[i + 2];
				if (is_digit(ord2) || is_ascii_upper_case(ord2)) {
					char bytes[3] = { (char)ord1, (char)ord2, 0 };
					res += (char)strtol(bytes, nullptr, 16);
					i += 2;
				}
			} else {
				res += src[i];
			}
		} else if (src[i] == '+') {
			res += ' ';
		} else {
			res += src[i];
		}
	}
	return String::utf8(res);
}

String String::c_unescape() const {
	String escaped = *this;
	escaped = escaped.replace("\\a", "\a");
	escaped = escaped.replace("\\b", "\b");
	escaped = escaped.replace("\\f", "\f");
	escaped = escaped.replace("\\n", "\n");
	escaped = escaped.replace("\\r", "\r");
	escaped = escaped.replace("\\t", "\t");
	escaped = escaped.replace("\\v", "\v");
	escaped = escaped.replace("\\'", "\'");
	escaped = escaped.replace("\\\"", "\"");
	escaped = escaped.replace("\\\\", "\\");

	return escaped;
}

String String::c_escape() const {
	String escaped = *this;
	escaped = escaped.replace("\\", "\\\\");
	escaped = escaped.replace("\a", "\\a");
	escaped = escaped.replace("\b", "\\b");
	escaped = escaped.replace("\f", "\\f");
	escaped = escaped.replace("\n", "\\n");
	escaped = escaped.replace("\r", "\\r");
	escaped = escaped.replace("\t", "\\t");
	escaped = escaped.replace("\v", "\\v");
	escaped = escaped.replace("\'", "\\'");
	escaped = escaped.replace("\"", "\\\"");

	return escaped;
}

String String::c_escape_multiline() const {
	String escaped = *this;
	escaped = escaped.replace("\\", "\\\\");
	escaped = escaped.replace("\"", "\\\"");

	return escaped;
}

String String::json_escape() const {
	String escaped = *this;
	escaped = escaped.replace("\\", "\\\\");
	escaped = escaped.replace("\b", "\\b");
	escaped = escaped.replace("\f", "\\f");
	escaped = escaped.replace("\n", "\\n");
	escaped = escaped.replace("\r", "\\r");
	escaped = escaped.replace("\t", "\\t");
	escaped = escaped.replace("\v", "\\v");
	escaped = escaped.replace("\"", "\\\"");

	return escaped;
}

String String::xml_escape(bool p_escape_quotes) const {
	String str = *this;
	str = str.replace("&", "&amp;");
	str = str.replace("<", "&lt;");
	str = str.replace(">", "&gt;");
	if (p_escape_quotes) {
		str = str.replace("'", "&apos;");
		str = str.replace("\"", "&quot;");
	}
	/*
for (int i=1;i<32;i++) {
	char chr[2]={i,0};
	str=str.replace(chr,"&#"+String::num(i)+";");
}*/
	return str;
}

static _FORCE_INLINE_ int _xml_unescape(const char32_t *p_src, int p_src_len, char32_t *p_dst) {
	int len = 0;
	while (p_src_len) {
		if (*p_src == '&') {
			int eat = 0;

			if (p_src_len >= 4 && p_src[1] == '#') {
				char32_t c = 0;
				bool overflow = false;
				if (p_src[2] == 'x') {
					// Hex entity &#x<num>;
					for (int i = 3; i < p_src_len; i++) {
						eat = i + 1;
						char32_t ct = p_src[i];
						if (ct == ';') {
							break;
						} else if (is_digit(ct)) {
							ct = ct - '0';
						} else if (ct >= 'a' && ct <= 'f') {
							ct = (ct - 'a') + 10;
						} else if (ct >= 'A' && ct <= 'F') {
							ct = (ct - 'A') + 10;
						} else {
							break;
						}
						if (c > (UINT32_MAX >> 4)) {
							overflow = true;
							break;
						}
						c <<= 4;
						c |= ct;
					}
				} else {
					// Decimal entity &#<num>;
					for (int i = 2; i < p_src_len; i++) {
						eat = i + 1;
						char32_t ct = p_src[i];
						if (ct == ';' || !is_digit(ct)) {
							break;
						}
					}
					if (p_src[eat - 1] == ';') {
						int64_t val = String::to_int(p_src + 2, eat - 3);
						if (val > 0 && val <= UINT32_MAX) {
							c = (char32_t)val;
						} else {
							overflow = true;
						}
					}
				}

				// Value must be non-zero, in the range of char32_t,
				// actually end with ';'. If invalid, leave the entity as-is
				if (c == '\0' || overflow || p_src[eat - 1] != ';') {
					eat = 1;
					c = *p_src;
				}
				if (p_dst) {
					*p_dst = c;
				}

			} else if (p_src_len >= 4 && p_src[1] == 'g' && p_src[2] == 't' && p_src[3] == ';') {
				if (p_dst) {
					*p_dst = '>';
				}
				eat = 4;
			} else if (p_src_len >= 4 && p_src[1] == 'l' && p_src[2] == 't' && p_src[3] == ';') {
				if (p_dst) {
					*p_dst = '<';
				}
				eat = 4;
			} else if (p_src_len >= 5 && p_src[1] == 'a' && p_src[2] == 'm' && p_src[3] == 'p' && p_src[4] == ';') {
				if (p_dst) {
					*p_dst = '&';
				}
				eat = 5;
			} else if (p_src_len >= 6 && p_src[1] == 'q' && p_src[2] == 'u' && p_src[3] == 'o' && p_src[4] == 't' && p_src[5] == ';') {
				if (p_dst) {
					*p_dst = '"';
				}
				eat = 6;
			} else if (p_src_len >= 6 && p_src[1] == 'a' && p_src[2] == 'p' && p_src[3] == 'o' && p_src[4] == 's' && p_src[5] == ';') {
				if (p_dst) {
					*p_dst = '\'';
				}
				eat = 6;
			} else {
				if (p_dst) {
					*p_dst = *p_src;
				}
				eat = 1;
			}

			if (p_dst) {
				p_dst++;
			}

			len++;
			p_src += eat;
			p_src_len -= eat;
		} else {
			if (p_dst) {
				*p_dst = *p_src;
				p_dst++;
			}
			len++;
			p_src++;
			p_src_len--;
		}
	}

	return len;
}

String String::xml_unescape() const {
	String str;
	int l = length();
	int len = _xml_unescape(get_data(), l, nullptr);
	if (len == 0) {
		return String();
	}
	str.resize(len + 1);
	char32_t *str_ptrw = str.ptrw();
	_xml_unescape(get_data(), l, str_ptrw);
	str_ptrw[len] = 0;
	return str;
}

String String::pad_decimals(int p_digits) const {
	String s = *this;
	int c = s.find_char('.');

	if (c == -1) {
		if (p_digits <= 0) {
			return s;
		}
		s += ".";
		c = s.length() - 1;
	} else {
		if (p_digits <= 0) {
			return s.substr(0, c);
		}
	}

	if (s.length() - (c + 1) > p_digits) {
		return s.substr(0, c + p_digits + 1);
	} else {
		int zeros_to_add = p_digits - s.length() + (c + 1);
		return s + String("0").repeat(zeros_to_add);
	}
}

String String::pad_zeros(int p_digits) const {
	String s = *this;
	int end = s.find_char('.');

	if (end == -1) {
		end = s.length();
	}

	if (end == 0) {
		return s;
	}

	int begin = 0;

	while (begin < end && !is_digit(s[begin])) {
		begin++;
	}

	int zeros_to_add = p_digits - (end - begin);

	if (zeros_to_add <= 0) {
		return s;
	} else {
		return s.insert(begin, String("0").repeat(zeros_to_add));
	}
}

String String::trim_prefix(const String &p_prefix) const {
	String s = *this;
	if (s.begins_with(p_prefix)) {
		return s.substr(p_prefix.length(), s.length() - p_prefix.length());
	}
	return s;
}

String String::trim_prefix(const char *p_prefix) const {
	String s = *this;
	if (s.begins_with(p_prefix)) {
		int prefix_length = strlen(p_prefix);
		return s.substr(prefix_length, s.length() - prefix_length);
	}
	return s;
}

String String::trim_suffix(const String &p_suffix) const {
	String s = *this;
	if (s.ends_with(p_suffix)) {
		return s.substr(0, s.length() - p_suffix.length());
	}
	return s;
}

String String::trim_suffix(const char *p_suffix) const {
	String s = *this;
	if (s.ends_with(p_suffix)) {
		return s.substr(0, s.length() - strlen(p_suffix));
	}
	return s;
}

bool String::is_valid_int() const {
	int len = length();

	if (len == 0) {
		return false;
	}

	int from = 0;
	if (len != 1 && (operator[](0) == '+' || operator[](0) == '-')) {
		from++;
	}

	for (int i = from; i < len; i++) {
		if (!is_digit(operator[](i))) {
			return false; // no start with number plz
		}
	}

	return true;
}

bool String::is_valid_hex_number(bool p_with_prefix) const {
	int len = length();

	if (len == 0) {
		return false;
	}

	int from = 0;
	if (len != 1 && (operator[](0) == '+' || operator[](0) == '-')) {
		from++;
	}

	if (p_with_prefix) {
		if (len < 3) {
			return false;
		}
		if (operator[](from) != '0' || operator[](from + 1) != 'x') {
			return false;
		}
		from += 2;
	}

	for (int i = from; i < len; i++) {
		char32_t c = operator[](i);
		if (is_hex_digit(c)) {
			continue;
		}
		return false;
	}

	return true;
}

bool String::is_valid_float() const {
	int len = length();

	if (len == 0) {
		return false;
	}

	int from = 0;
	if (operator[](0) == '+' || operator[](0) == '-') {
		from++;
	}

	bool exponent_found = false;
	bool period_found = false;
	bool sign_found = false;
	bool exponent_values_found = false;
	bool numbers_found = false;

	for (int i = from; i < len; i++) {
		if (is_digit(operator[](i))) {
			if (exponent_found) {
				exponent_values_found = true;
			} else {
				numbers_found = true;
			}
		} else if (numbers_found && !exponent_found && operator[](i) == 'e') {
			exponent_found = true;
		} else if (!period_found && !exponent_found && operator[](i) == '.') {
			period_found = true;
		} else if ((operator[](i) == '-' || operator[](i) == '+') && exponent_found && !exponent_values_found && !sign_found) {
			sign_found = true;
		} else {
			return false; // no start with number plz
		}
	}

	return numbers_found;
}

String String::path_to_file(const String &p_path) const {
	// Don't get base dir for src, this is expected to be a dir already.
	String src = replace("\\", "/");
	String dst = p_path.replace("\\", "/").get_base_dir();
	String rel = src.path_to(dst);
	if (rel == dst) { // failed
		return p_path;
	} else {
		return rel + p_path.get_file();
	}
}

String String::path_to(const String &p_path) const {
	String src = replace("\\", "/");
	String dst = p_path.replace("\\", "/");
	if (!src.ends_with("/")) {
		src += "/";
	}
	if (!dst.ends_with("/")) {
		dst += "/";
	}

	if (src.begins_with("res://") && dst.begins_with("res://")) {
		src = src.replace("res://", "/");
		dst = dst.replace("res://", "/");

	} else if (src.begins_with("user://") && dst.begins_with("user://")) {
		src = src.replace("user://", "/");
		dst = dst.replace("user://", "/");

	} else if (src.begins_with("/") && dst.begins_with("/")) {
		//nothing
	} else {
		//dos style
		String src_begin = src.get_slicec('/', 0);
		String dst_begin = dst.get_slicec('/', 0);

		if (src_begin != dst_begin) {
			return p_path; //impossible to do this
		}

		src = src.substr(src_begin.length(), src.length());
		dst = dst.substr(dst_begin.length(), dst.length());
	}

	//remove leading and trailing slash and split
	Vector<String> src_dirs = src.substr(1, src.length() - 2).split("/");
	Vector<String> dst_dirs = dst.substr(1, dst.length() - 2).split("/");

	//find common parent
	int common_parent = 0;

	while (true) {
		if (src_dirs.size() == common_parent) {
			break;
		}
		if (dst_dirs.size() == common_parent) {
			break;
		}
		if (src_dirs[common_parent] != dst_dirs[common_parent]) {
			break;
		}
		common_parent++;
	}

	common_parent--;

	int dirs_to_backtrack = (src_dirs.size() - 1) - common_parent;
	String dir = String("../").repeat(dirs_to_backtrack);

	for (int i = common_parent + 1; i < dst_dirs.size(); i++) {
		dir += dst_dirs[i] + "/";
	}

	if (dir.length() == 0) {
		dir = "./";
	}
	return dir;
}

bool String::is_valid_html_color() const {
	return Color::html_is_valid(*this);
}

// Changes made to the set of invalid filename characters must also be reflected in the String documentation for is_valid_filename.
static const char *invalid_filename_characters[] = { ":", "/", "\\", "?", "*", "\"", "|", "%", "<", ">" };

bool String::is_valid_filename() const {
	String stripped = strip_edges();
	if (*this != stripped) {
		return false;
	}

	if (stripped.is_empty()) {
		return false;
	}

	for (const char *ch : invalid_filename_characters) {
		if (contains(ch)) {
			return false;
		}
	}
	return true;
}

String String::validate_filename() const {
	String name = strip_edges();
	for (const char *ch : invalid_filename_characters) {
		name = name.replace(ch, "_");
	}
	return name;
}

bool String::is_valid_ip_address() const {
	if (find_char(':') >= 0) {
		Vector<String> ip = split(":");
		for (int i = 0; i < ip.size(); i++) {
			const String &n = ip[i];
			if (n.is_empty()) {
				continue;
			}
			if (n.is_valid_hex_number(false)) {
				int64_t nint = n.hex_to_int();
				if (nint < 0 || nint > 0xffff) {
					return false;
				}
				continue;
			}
			if (!n.is_valid_ip_address()) {
				return false;
			}
		}

	} else {
		Vector<String> ip = split(".");
		if (ip.size() != 4) {
			return false;
		}
		for (int i = 0; i < ip.size(); i++) {
			const String &n = ip[i];
			if (!n.is_valid_int()) {
				return false;
			}
			int val = n.to_int();
			if (val < 0 || val > 255) {
				return false;
			}
		}
	}

	return true;
}

bool String::is_resource_file() const {
	return begins_with("res://") && find("::") == -1;
}

bool String::is_relative_path() const {
	return !is_absolute_path();
}

String String::get_base_dir() const {
	int end = 0;

	// URL scheme style base.
	int basepos = find("://");
	if (basepos != -1) {
		end = basepos + 3;
	}

	// Windows top level directory base.
	if (end == 0) {
		basepos = find(":/");
		if (basepos == -1) {
			basepos = find(":\\");
		}
		if (basepos != -1) {
			end = basepos + 2;
		}
	}

	// Windows UNC network share path.
	if (end == 0) {
		if (is_network_share_path()) {
			basepos = find_char('/', 2);
			if (basepos == -1) {
				basepos = find_char('\\', 2);
			}
			int servpos = find_char('/', basepos + 1);
			if (servpos == -1) {
				servpos = find_char('\\', basepos + 1);
			}
			if (servpos != -1) {
				end = servpos + 1;
			}
		}
	}

	// Unix root directory base.
	if (end == 0) {
		if (begins_with("/")) {
			end = 1;
		}
	}

	String rs;
	String base;
	if (end != 0) {
		rs = substr(end, length());
		base = substr(0, end);
	} else {
		rs = *this;
	}

	int sep = MAX(rs.rfind_char('/'), rs.rfind_char('\\'));
	if (sep == -1) {
		return base;
	}

	return base + rs.substr(0, sep);
}

String String::get_file() const {
	int sep = MAX(rfind_char('/'), rfind_char('\\'));
	if (sep == -1) {
		return *this;
	}

	return substr(sep + 1, length());
}

String String::get_extension() const {
	int pos = rfind_char('.');
	if (pos < 0 || pos < MAX(rfind_char('/'), rfind_char('\\'))) {
		return "";
	}

	return substr(pos + 1, length());
}

String String::path_join(const String &p_file) const {
	if (is_empty()) {
		return p_file;
	}
	if (operator[](length() - 1) == '/' || (p_file.size() > 0 && p_file.operator[](0) == '/')) {
		return *this + p_file;
	}
	return *this + "/" + p_file;
}

String String::property_name_encode() const {
	// Escape and quote strings with extended ASCII or further Unicode characters
	// as well as '"', '=' or ' ' (32)
	const char32_t *cstr = get_data();
	for (int i = 0; cstr[i]; i++) {
		if (cstr[i] == '=' || cstr[i] == '"' || cstr[i] == ';' || cstr[i] == '[' || cstr[i] == ']' || cstr[i] < 33 || cstr[i] > 126) {
			return "\"" + c_escape_multiline() + "\"";
		}
	}
	// Keep as is
	return *this;
}

// Changes made to the set of invalid characters must also be reflected in the String documentation.

static const char32_t invalid_node_name_characters[] = { '.', ':', '@', '/', '\"', UNIQUE_NODE_PREFIX[0], 0 };

String String::get_invalid_node_name_characters(bool p_allow_internal) {
	// Do not use this function for critical validation.
	String r;
	const char32_t *c = invalid_node_name_characters;
	while (*c) {
		if (p_allow_internal && *c == '@') {
			c++;
			continue;
		}

		if (c != invalid_node_name_characters) {
			r += " ";
		}
		r += String::chr(*c);
		c++;
	}
	return r;
}

String String::validate_node_name() const {
	// This is a critical validation in node addition, so it must be optimized.
	const char32_t *cn = ptr();
	if (cn == nullptr) {
		return String();
	}
	bool valid = true;
	uint32_t idx = 0;
	while (cn[idx]) {
		const char32_t *c = invalid_node_name_characters;
		while (*c) {
			if (cn[idx] == *c) {
				valid = false;
				break;
			}
			c++;
		}
		if (!valid) {
			break;
		}
		idx++;
	}

	if (valid) {
		return *this;
	}

	String validated = *this;
	char32_t *nn = validated.ptrw();
	while (nn[idx]) {
		const char32_t *c = invalid_node_name_characters;
		while (*c) {
			if (nn[idx] == *c) {
				nn[idx] = '_';
				break;
			}
			c++;
		}
		idx++;
	}

	return validated;
}

String String::get_basename() const {
	int pos = rfind_char('.');
	if (pos < 0 || pos < MAX(rfind_char('/'), rfind_char('\\'))) {
		return *this;
	}

	return substr(0, pos);
}

String itos(int64_t p_val) {
	return String::num_int64(p_val);
}

String uitos(uint64_t p_val) {
	return String::num_uint64(p_val);
}

String rtos(double p_val) {
	return String::num(p_val);
}

String rtoss(double p_val) {
	return String::num_scientific(p_val);
}

// Right-pad with a character.
String String::rpad(int min_length, const String &character) const {
	String s = *this;
	int padding = min_length - s.length();
	if (padding > 0) {
		s += character.repeat(padding);
	}
	return s;
}

// Left-pad with a character.
String String::lpad(int min_length, const String &character) const {
	String s = *this;
	int padding = min_length - s.length();
	if (padding > 0) {
		s = character.repeat(padding) + s;
	}
	return s;
}

// sprintf is implemented in GDScript via:
//   "fish %s pie" % "frog"
//   "fish %s %d pie" % ["frog", 12]
// In case of an error, the string returned is the error description and "error" is true.
String String::sprintf(const Array &values, bool *error) const {
	static const String ZERO("0");
	static const String SPACE(" ");
	static const String MINUS("-");
	static const String PLUS("+");

	String formatted;
	char32_t *self = (char32_t *)get_data();
	bool in_format = false;
	int value_index = 0;
	int min_chars = 0;
	int min_decimals = 0;
	bool in_decimals = false;
	bool pad_with_zeros = false;
	bool left_justified = false;
	bool show_sign = false;
	bool as_unsigned = false;

	if (error) {
		*error = true;
	}

	for (; *self; self++) {
		const char32_t c = *self;

		if (in_format) { // We have % - let's see what else we get.
			switch (c) {
				case '%': { // Replace %% with %
					formatted += c;
					in_format = false;
					break;
				}
				case 'd': // Integer (signed)
				case 'o': // Octal
				case 'x': // Hexadecimal (lowercase)
				case 'X': { // Hexadecimal (uppercase)
					if (value_index >= values.size()) {
						return "not enough arguments for format string";
					}

					if (!values[value_index].is_num()) {
						return "a number is required";
					}

					int64_t value = values[value_index];
					int base = 16;
					bool capitalize = false;
					switch (c) {
						case 'd':
							base = 10;
							break;
						case 'o':
							base = 8;
							break;
						case 'x':
							break;
						case 'X':
							capitalize = true;
							break;
					}
					// Get basic number.
					String str;
					if (!as_unsigned) {
						str = String::num_int64(ABS(value), base, capitalize);
					} else {
						uint64_t uvalue = *((uint64_t *)&value);
						// In unsigned hex, if the value fits in 32 bits, trim it down to that.
						if (base == 16 && value < 0 && value >= INT32_MIN) {
							uvalue &= 0xffffffff;
						}
						str = String::num_uint64(uvalue, base, capitalize);
					}
					int number_len = str.length();

					bool negative = value < 0 && !as_unsigned;

					// Padding.
					int pad_chars_count = (negative || show_sign) ? min_chars - 1 : min_chars;
					const String &pad_char = pad_with_zeros ? ZERO : SPACE;
					if (left_justified) {
						str = str.rpad(pad_chars_count, pad_char);
					} else {
						str = str.lpad(pad_chars_count, pad_char);
					}

					// Sign.
					if (show_sign || negative) {
						const String &sign_char = negative ? MINUS : PLUS;
						if (left_justified) {
							str = str.insert(0, sign_char);
						} else {
							str = str.insert(pad_with_zeros ? 0 : str.length() - number_len, sign_char);
						}
					}

					formatted += str;
					++value_index;
					in_format = false;

					break;
				}
				case 'f': { // Float
					if (value_index >= values.size()) {
						return "not enough arguments for format string";
					}

					if (!values[value_index].is_num()) {
						return "a number is required";
					}

					double value = values[value_index];
					bool is_negative = signbit(value);
					String str = String::num(Math::abs(value), min_decimals);
					const bool is_finite = Math::is_finite(value);

					// Pad decimals out.
					if (is_finite) {
						str = str.pad_decimals(min_decimals);
					}

					int initial_len = str.length();

					// Padding. Leave room for sign later if required.
					int pad_chars_count = (is_negative || show_sign) ? min_chars - 1 : min_chars;
					const String &pad_char = (pad_with_zeros && is_finite) ? ZERO : SPACE; // Never pad NaN or inf with zeros
					if (left_justified) {
						str = str.rpad(pad_chars_count, pad_char);
					} else {
						str = str.lpad(pad_chars_count, pad_char);
					}

					// Add sign if needed.
					if (show_sign || is_negative) {
						const String &sign_char = is_negative ? MINUS : PLUS;
						if (left_justified) {
							str = str.insert(0, sign_char);
						} else {
							str = str.insert(pad_with_zeros ? 0 : str.length() - initial_len, sign_char);
						}
					}

					formatted += str;
					++value_index;
					in_format = false;
					break;
				}
				case 'v': { // Vector2/3/4/2i/3i/4i
					if (value_index >= values.size()) {
						return "not enough arguments for format string";
					}

					int count;
					switch (values[value_index].get_type()) {
						case Variant::VECTOR2:
						case Variant::VECTOR2I: {
							count = 2;
						} break;
						case Variant::VECTOR3:
						case Variant::VECTOR3I: {
							count = 3;
						} break;
						case Variant::VECTOR4:
						case Variant::VECTOR4I: {
							count = 4;
						} break;
						default: {
							return "%v requires a vector type (Vector2/3/4/2i/3i/4i)";
						}
					}

					Vector4 vec = values[value_index];
					String str = "(";
					for (int i = 0; i < count; i++) {
						double val = vec[i];
						String number_str = String::num(Math::abs(val), min_decimals);
						const bool is_finite = Math::is_finite(val);

						// Pad decimals out.
						if (is_finite) {
							number_str = number_str.pad_decimals(min_decimals);
						}

						int initial_len = number_str.length();

						// Padding. Leave room for sign later if required.
						int pad_chars_count = val < 0 ? min_chars - 1 : min_chars;
						const String &pad_char = (pad_with_zeros && is_finite) ? ZERO : SPACE; // Never pad NaN or inf with zeros
						if (left_justified) {
							number_str = number_str.rpad(pad_chars_count, pad_char);
						} else {
							number_str = number_str.lpad(pad_chars_count, pad_char);
						}

						// Add sign if needed.
						if (val < 0) {
							if (left_justified) {
								number_str = number_str.insert(0, MINUS);
							} else {
								number_str = number_str.insert(pad_with_zeros ? 0 : number_str.length() - initial_len, MINUS);
							}
						}

						// Add number to combined string
						str += number_str;

						if (i < count - 1) {
							str += ", ";
						}
					}
					str += ")";

					formatted += str;
					++value_index;
					in_format = false;
					break;
				}
				case 's': { // String
					if (value_index >= values.size()) {
						return "not enough arguments for format string";
					}

					String str = values[value_index];
					// Padding.
					if (left_justified) {
						str = str.rpad(min_chars);
					} else {
						str = str.lpad(min_chars);
					}

					formatted += str;
					++value_index;
					in_format = false;
					break;
				}
				case 'c': {
					if (value_index >= values.size()) {
						return "not enough arguments for format string";
					}

					// Convert to character.
					String str;
					if (values[value_index].is_num()) {
						int value = values[value_index];
						if (value < 0) {
							return "unsigned integer is lower than minimum";
						} else if (value >= 0xd800 && value <= 0xdfff) {
							return "unsigned integer is invalid Unicode character";
						} else if (value > 0x10ffff) {
							return "unsigned integer is greater than maximum";
						}
						str = chr(values[value_index]);
					} else if (values[value_index].get_type() == Variant::STRING) {
						str = values[value_index];
						if (str.length() != 1) {
							return "%c requires number or single-character string";
						}
					} else {
						return "%c requires number or single-character string";
					}

					// Padding.
					if (left_justified) {
						str = str.rpad(min_chars);
					} else {
						str = str.lpad(min_chars);
					}

					formatted += str;
					++value_index;
					in_format = false;
					break;
				}
				case '-': { // Left justify
					left_justified = true;
					break;
				}
				case '+': { // Show + if positive.
					show_sign = true;
					break;
				}
				case 'u': { // Treat as unsigned (for int/hex).
					as_unsigned = true;
					break;
				}
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9': {
					int n = c - '0';
					if (in_decimals) {
						min_decimals *= 10;
						min_decimals += n;
					} else {
						if (c == '0' && min_chars == 0) {
							if (left_justified) {
								WARN_PRINT("'0' flag ignored with '-' flag in string format");
							} else {
								pad_with_zeros = true;
							}
						} else {
							min_chars *= 10;
							min_chars += n;
						}
					}
					break;
				}
				case '.': { // Float/Vector separator.
					if (in_decimals) {
						return "too many decimal points in format";
					}
					in_decimals = true;
					min_decimals = 0; // We want to add the value manually.
					break;
				}

				case '*': { // Dynamic width, based on value.
					if (value_index >= values.size()) {
						return "not enough arguments for format string";
					}

					Variant::Type value_type = values[value_index].get_type();
					if (!values[value_index].is_num() &&
							value_type != Variant::VECTOR2 && value_type != Variant::VECTOR2I &&
							value_type != Variant::VECTOR3 && value_type != Variant::VECTOR3I &&
							value_type != Variant::VECTOR4 && value_type != Variant::VECTOR4I) {
						return "* wants number or vector";
					}

					int size = values[value_index];

					if (in_decimals) {
						min_decimals = size;
					} else {
						min_chars = size;
					}

					++value_index;
					break;
				}

				default: {
					return "unsupported format character";
				}
			}
		} else { // Not in format string.
			switch (c) {
				case '%':
					in_format = true;
					// Back to defaults:
					min_chars = 0;
					min_decimals = 6;
					pad_with_zeros = false;
					left_justified = false;
					show_sign = false;
					in_decimals = false;
					break;
				default:
					formatted += c;
			}
		}
	}

	if (in_format) {
		return "incomplete format";
	}

	if (value_index != values.size()) {
		return "not all arguments converted during string formatting";
	}

	if (error) {
		*error = false;
	}
	return formatted;
}

String String::quote(const String &quotechar) const {
	return quotechar + *this + quotechar;
}

String String::unquote() const {
	if (!is_quoted()) {
		return *this;
	}

	return substr(1, length() - 2);
}

Vector<uint8_t> String::to_ascii_buffer() const {
	const String *s = this;
	if (s->is_empty()) {
		return Vector<uint8_t>();
	}
	CharString charstr = s->ascii();

	Vector<uint8_t> retval;
	size_t len = charstr.length();
	retval.resize(len);
	uint8_t *w = retval.ptrw();
	memcpy(w, charstr.ptr(), len);

	return retval;
}

Vector<uint8_t> String::to_utf8_buffer() const {
	const String *s = this;
	if (s->is_empty()) {
		return Vector<uint8_t>();
	}
	CharString charstr = s->utf8();

	Vector<uint8_t> retval;
	size_t len = charstr.length();
	retval.resize(len);
	uint8_t *w = retval.ptrw();
	memcpy(w, charstr.ptr(), len);

	return retval;
}

Vector<uint8_t> String::to_utf16_buffer() const {
	const String *s = this;
	if (s->is_empty()) {
		return Vector<uint8_t>();
	}
	Char16String charstr = s->utf16();

	Vector<uint8_t> retval;
	size_t len = charstr.length() * sizeof(char16_t);
	retval.resize(len);
	uint8_t *w = retval.ptrw();
	memcpy(w, (const void *)charstr.ptr(), len);

	return retval;
}

Vector<uint8_t> String::to_utf32_buffer() const {
	const String *s = this;
	if (s->is_empty()) {
		return Vector<uint8_t>();
	}

	Vector<uint8_t> retval;
	size_t len = s->length() * sizeof(char32_t);
	retval.resize(len);
	uint8_t *w = retval.ptrw();
	memcpy(w, (const void *)s->ptr(), len);

	return retval;
}

Vector<uint8_t> String::to_wchar_buffer() const {
#ifdef WINDOWS_ENABLED
	return to_utf16_buffer();
#else
	return to_utf32_buffer();
#endif
}

#ifdef TOOLS_ENABLED
/**
 * "Tools TRanslate". Performs string replacement for internationalization
 * within the editor. A translation context can optionally be specified to
 * disambiguate between identical source strings in translations. When
 * placeholders are desired, use `vformat(TTR("Example: %s"), some_string)`.
 * If a string mentions a quantity (and may therefore need a dynamic plural form),
 * use `TTRN()` instead of `TTR()`.
 *
 * NOTE: Only use `TTR()` in editor-only code (typically within the `editor/` folder).
 * For translations that can be supplied by exported projects, use `RTR()` instead.
 */
String TTR(const String &p_text, const String &p_context) {
	if (TranslationServer::get_singleton()) {
		return TranslationServer::get_singleton()->tool_translate(p_text, p_context);
	}

	return p_text;
}

/**
 * "Tools TRanslate for N items". Performs string replacement for
 * internationalization within the editor. A translation context can optionally
 * be specified to disambiguate between identical source strings in
 * translations. Use `TTR()` if the string doesn't need dynamic plural form.
 * When placeholders are desired, use
 * `vformat(TTRN("%d item", "%d items", some_integer), some_integer)`.
 * The placeholder must be present in both strings to avoid run-time warnings in `vformat()`.
 *
 * NOTE: Only use `TTRN()` in editor-only code (typically within the `editor/` folder).
 * For translations that can be supplied by exported projects, use `RTRN()` instead.
 */
String TTRN(const String &p_text, const String &p_text_plural, int p_n, const String &p_context) {
	if (TranslationServer::get_singleton()) {
		return TranslationServer::get_singleton()->tool_translate_plural(p_text, p_text_plural, p_n, p_context);
	}

	// Return message based on English plural rule if translation is not possible.
	if (p_n == 1) {
		return p_text;
	}
	return p_text_plural;
}

/**
 * "Docs TRanslate". Used for the editor class reference documentation,
 * handling descriptions extracted from the XML.
 * It also replaces `$DOCS_URL` with the actual URL to the documentation's branch,
 * to allow dehardcoding it in the XML and doing proper substitutions everywhere.
 */
String DTR(const String &p_text, const String &p_context) {
	// Comes straight from the XML, so remove indentation and any trailing whitespace.
	const String text = p_text.dedent().strip_edges();

	if (TranslationServer::get_singleton()) {
		return String(TranslationServer::get_singleton()->doc_translate(text, p_context)).replace("$DOCS_URL", VERSION_DOCS_URL);
	}

	return text.replace("$DOCS_URL", VERSION_DOCS_URL);
}

/**
 * "Docs TRanslate for N items". Used for the editor class reference documentation
 * (with support for plurals), handling descriptions extracted from the XML.
 * It also replaces `$DOCS_URL` with the actual URL to the documentation's branch,
 * to allow dehardcoding it in the XML and doing proper substitutions everywhere.
 */
String DTRN(const String &p_text, const String &p_text_plural, int p_n, const String &p_context) {
	const String text = p_text.dedent().strip_edges();
	const String text_plural = p_text_plural.dedent().strip_edges();

	if (TranslationServer::get_singleton()) {
		return String(TranslationServer::get_singleton()->doc_translate_plural(text, text_plural, p_n, p_context)).replace("$DOCS_URL", VERSION_DOCS_URL);
	}

	// Return message based on English plural rule if translation is not possible.
	if (p_n == 1) {
		return text.replace("$DOCS_URL", VERSION_DOCS_URL);
	}
	return text_plural.replace("$DOCS_URL", VERSION_DOCS_URL);
}
#endif

/**
 * "Run-time TRanslate". Performs string replacement for internationalization
 * without the editor. A translation context can optionally be specified to
 * disambiguate between identical source strings in translations. When
 * placeholders are desired, use `vformat(RTR("Example: %s"), some_string)`.
 * If a string mentions a quantity (and may therefore need a dynamic plural form),
 * use `RTRN()` instead of `RTR()`.
 *
 * NOTE: Do not use `RTR()` in editor-only code (typically within the `editor/`
 * folder). For editor translations, use `TTR()` instead.
 */
String RTR(const String &p_text, const String &p_context) {
	if (TranslationServer::get_singleton()) {
		String rtr = TranslationServer::get_singleton()->tool_translate(p_text, p_context);
		if (rtr.is_empty() || rtr == p_text) {
			return TranslationServer::get_singleton()->translate(p_text, p_context);
		}
		return rtr;
	}

	return p_text;
}

/**
 * "Run-time TRanslate for N items". Performs string replacement for
 * internationalization without the editor. A translation context can optionally
 * be specified to disambiguate between identical source strings in translations.
 * Use `RTR()` if the string doesn't need dynamic plural form. When placeholders
 * are desired, use `vformat(RTRN("%d item", "%d items", some_integer), some_integer)`.
 * The placeholder must be present in both strings to avoid run-time warnings in `vformat()`.
 *
 * NOTE: Do not use `RTRN()` in editor-only code (typically within the `editor/`
 * folder). For editor translations, use `TTRN()` instead.
 */
String RTRN(const String &p_text, const String &p_text_plural, int p_n, const String &p_context) {
	if (TranslationServer::get_singleton()) {
		String rtr = TranslationServer::get_singleton()->tool_translate_plural(p_text, p_text_plural, p_n, p_context);
		if (rtr.is_empty() || rtr == p_text || rtr == p_text_plural) {
			return TranslationServer::get_singleton()->translate_plural(p_text, p_text_plural, p_n, p_context);
		}
		return rtr;
	}

	// Return message based on English plural rule if translation is not possible.
	if (p_n == 1) {
		return p_text;
	}
	return p_text_plural;
}
