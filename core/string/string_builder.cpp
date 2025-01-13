/**************************************************************************/
/*  string_builder.cpp                                                    */
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

#include "string_builder.h"

#include <string.h>

StringBuilder &StringBuilder::append(const String &p_string) {
	if (p_string.is_empty()) {
		return *this;
	}

	strings.push_back(p_string);
	appended_strings.push_back(-1);

	string_length += p_string.length();

	return *this;
}

StringBuilder &StringBuilder::append(const char *p_cstring) {
	int32_t len = strlen(p_cstring);

	c_strings.push_back(p_cstring);
	appended_strings.push_back(len);

	string_length += len;

	return *this;
}

String StringBuilder::as_string() const {
	if (string_length == 0) {
		return "";
	}

	String string;
	string.resize(string_length + 1);
	char32_t *buffer = string.ptrw();

	int current_position = 0;

	int godot_string_elem = 0;
	int c_string_elem = 0;

	for (uint32_t i = 0; i < appended_strings.size(); i++) {
		const int32_t str_len = appended_strings[i];

		if (str_len == -1) {
			// Godot string
			const String &s = strings[godot_string_elem];

			memcpy(buffer + current_position, s.ptr(), s.length() * sizeof(char32_t));

			current_position += s.length();

			godot_string_elem++;
		} else {
			const char *s = c_strings[c_string_elem];

			for (int32_t j = 0; j < str_len; j++) {
				buffer[current_position + j] = s[j];
			}

			current_position += str_len;

			c_string_elem++;
		}
	}
	buffer[current_position] = 0;

	return string;
}
