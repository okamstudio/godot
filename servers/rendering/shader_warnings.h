/**************************************************************************/
/*  shader_warnings.h                                                     */
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

#ifndef SHADER_WARNINGS_H
#define SHADER_WARNINGS_H

#ifdef DEBUG_ENABLED

#include "core/string/string_name.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"

class ShaderWarning {
public:
	enum Code {
		FLOAT_COMPARISON,
		UNUSED_CONSTANT,
		UNUSED_FUNCTION,
		UNUSED_STRUCT,
		UNUSED_UNIFORM,
		UNUSED_VARYING,
		UNUSED_LOCAL_VARIABLE,
		FORMATTING_ERROR,
		DEVICE_LIMIT_EXCEEDED,
		MAGIC_POSITION_WRITE,
		WARNING_MAX,
	};

	enum CodeFlags : uint32_t {
		NONE_FLAG = 0U,
		FLOAT_COMPARISON_FLAG = 1U,
		UNUSED_CONSTANT_FLAG = 2U,
		UNUSED_FUNCTION_FLAG = 4U,
		UNUSED_STRUCT_FLAG = 8U,
		UNUSED_UNIFORM_FLAG = 16U,
		UNUSED_VARYING_FLAG = 32U,
		UNUSED_LOCAL_VARIABLE_FLAG = 64U,
		FORMATTING_ERROR_FLAG = 128U,
		DEVICE_LIMIT_EXCEEDED_FLAG = 256U,
		MAGIC_POSITION_WRITE_FLAG = 512U,
	};

private:
	Code code;
	int line;
	StringName subject;
	Vector<Variant> extra_args;

public:
	Code get_code() const;
	int get_line() const;
	const StringName &get_subject() const;
	String get_message() const;
	String get_name() const;
	Vector<Variant> get_extra_args() const;

	static String get_name_from_code(Code p_code);
	static Code get_code_from_name(const String &p_name);
	static CodeFlags get_flags_from_codemap(const HashMap<Code, bool> &p_map);

	ShaderWarning(Code p_code = WARNING_MAX, int p_line = -1, const StringName &p_subject = "", const Vector<Variant> &p_extra_args = Vector<Variant>());
};

#endif // DEBUG_ENABLED

#endif // SHADER_WARNINGS_H
