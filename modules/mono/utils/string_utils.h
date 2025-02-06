/**************************************************************************/
/*  string_utils.h                                                        */
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

#include "core/string/ustring.h"
#include "core/variant/variant.h"

#include <stdarg.h>

String sformat(const String &p_text, const String &p1 = String(), const String &p2 = String(),
		const String &p3 = String(), const String &p4 = String(), const String &p5 = String(), const String &p6 = String());

#ifdef TOOLS_ENABLED
bool is_csharp_keyword(const String &p_name);

String escape_csharp_keyword(const String &p_name);
#endif

Error read_all_file_utf8(const String &p_path, String &r_content);

#if defined(__GNUC__)
#define _PRINTF_FORMAT_ATTRIBUTE_1_0 __attribute__((format(printf, 1, 0)))
#define _PRINTF_FORMAT_ATTRIBUTE_1_2 __attribute__((format(printf, 1, 2)))
#else
#define _PRINTF_FORMAT_ATTRIBUTE_1_0
#define _PRINTF_FORMAT_ATTRIBUTE_1_2
#endif

String str_format(const char *p_format, ...) _PRINTF_FORMAT_ATTRIBUTE_1_2;
String str_format(const char *p_format, va_list p_list) _PRINTF_FORMAT_ATTRIBUTE_1_0;
char *str_format_new(const char *p_format, ...) _PRINTF_FORMAT_ATTRIBUTE_1_2;
char *str_format_new(const char *p_format, va_list p_list) _PRINTF_FORMAT_ATTRIBUTE_1_0;
