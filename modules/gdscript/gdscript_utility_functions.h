/**************************************************************************/
/*  gdscript_utility_functions.h                                          */
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

#include "core/string/string_name.h"
#include "core/variant/variant.h"

template <typename T>
class TypedArray;

class GDScriptUtilityFunctions {
public:
	typedef void (*FunctionPtr)(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error);

	static FunctionPtr get_function(const StringName &p_function);
	static bool has_function_return_value(const StringName &p_function);
	static Variant::Type get_function_return_type(const StringName &p_function);
	static StringName get_function_return_class(const StringName &p_function);
	static Variant::Type get_function_argument_type(const StringName &p_function, int p_arg);
	static int get_function_argument_count(const StringName &p_function);
	static bool is_function_vararg(const StringName &p_function);
	static bool is_function_constant(const StringName &p_function);

	static bool function_exists(const StringName &p_function);
	static void get_function_list(List<StringName> *r_functions);
	static MethodInfo get_function_info(const StringName &p_function);

	static void register_functions();
	static void unregister_functions();
};
