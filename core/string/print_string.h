/**************************************************************************/
/*  print_string.h                                                        */
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

#include "core/variant/variant.h"

extern void (*_print_func)(String);

typedef void (*PrintHandlerFunc)(void *, const String &p_string, bool p_error, bool p_rich);

struct PrintHandlerList {
	PrintHandlerFunc printfunc = nullptr;
	void *userdata = nullptr;

	PrintHandlerList *next = nullptr;

	PrintHandlerList() {}
};

String stringify_variants(const Variant &p_var);

template <typename... Args>
String stringify_variants(const Variant &p_var, Args... p_args) {
	return p_var.operator String() + " " + stringify_variants(p_args...);
}

void add_print_handler(PrintHandlerList *p_handler);
void remove_print_handler(const PrintHandlerList *p_handler);

extern void __print_line(const String &p_string);
extern void __print_line_rich(const String &p_string);
extern void print_error(const String &p_string);
extern bool is_print_verbose_enabled();

// This version avoids processing the text to be printed until it actually has to be printed, saving some CPU usage.
#define print_verbose(m_text)             \
	{                                     \
		if (is_print_verbose_enabled()) { \
			print_line(m_text);           \
		}                                 \
	}

inline void print_line(const Variant &v) {
	__print_line(stringify_variants(v));
}

inline void print_line_rich(const Variant &v) {
	__print_line_rich(stringify_variants(v));
}

template <typename... Args>
void print_line(const Variant &p_var, Args... p_args) {
	__print_line(stringify_variants(p_var, p_args...));
}

template <typename... Args>
void print_line_rich(const Variant &p_var, Args... p_args) {
	__print_line_rich(stringify_variants(p_var, p_args...));
}
