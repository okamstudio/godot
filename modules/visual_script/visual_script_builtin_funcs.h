/**************************************************************************/
/*  visual_script_builtin_funcs.h                                         */
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

#ifndef VISUAL_SCRIPT_BUILTIN_FUNCS_H
#define VISUAL_SCRIPT_BUILTIN_FUNCS_H

#include "visual_script.h"

class VisualScriptBuiltinFunc : public VisualScriptNode {
	GDCLASS(VisualScriptBuiltinFunc, VisualScriptNode);

public:
	enum BuiltinFunc {
		MATH_SIN,
		MATH_COS,
		MATH_TAN,
		MATH_SINH,
		MATH_COSH,
		MATH_TANH,
		MATH_ASIN,
		MATH_ACOS,
		MATH_ATAN,
		MATH_ATAN2,
		MATH_SQRT,
		MATH_FMOD,
		MATH_FPOSMOD,
		MATH_FLOOR,
		MATH_CEIL,
		MATH_ROUND,
		MATH_ABS,
		MATH_SIGN,
		MATH_POW,
		MATH_LOG,
		MATH_EXP,
		MATH_ISNAN,
		MATH_ISINF,
		MATH_EASE,
		MATH_DECIMALS,
		MATH_STEPIFY,
		MATH_LERP,
		MATH_INVERSE_LERP,
		MATH_RANGE_LERP,
		MATH_MOVE_TOWARD,
		MATH_DECTIME,
		MATH_RANDOMIZE,
		MATH_RAND,
		MATH_RANDF,
		MATH_RANDOM,
		MATH_SEED,
		MATH_RANDSEED,
		MATH_DEG2RAD,
		MATH_RAD2DEG,
		MATH_LINEAR2DB,
		MATH_DB2LINEAR,
		MATH_POLAR2CARTESIAN,
		MATH_CARTESIAN2POLAR,
		MATH_WRAP,
		MATH_WRAPF,
		LOGIC_MAX,
		LOGIC_MIN,
		LOGIC_CLAMP,
		LOGIC_NEAREST_PO2,
		OBJ_WEAKREF,
		FUNC_FUNCREF,
		TYPE_CONVERT,
		TYPE_OF,
		TYPE_EXISTS,
		TEXT_CHAR,
		TEXT_STR,
		TEXT_PRINT,
		TEXT_PRINTERR,
		TEXT_PRINTRAW,
		VAR_TO_STR,
		STR_TO_VAR,
		VAR_TO_BYTES,
		BYTES_TO_VAR,
		COLORN,
		MATH_SMOOTHSTEP,
		MATH_POSMOD,
		MATH_LERP_ANGLE,
		TEXT_ORD,
		TEXT_PRINT_VERBOSE,
		FUNC_MAX
	};

	static int get_func_argument_count(BuiltinFunc p_func);
	static String get_func_name(BuiltinFunc p_func);
	static void exec_func(BuiltinFunc p_func, const Variant **p_inputs, Variant *r_return, Variant::CallError &r_error, String &r_error_str);
	static BuiltinFunc find_function(const String &p_string);

private:
	static const char *func_name[FUNC_MAX];
	BuiltinFunc func;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const;
	virtual bool has_input_sequence_port() const;

	virtual String get_output_sequence_port_text(int p_port) const;

	virtual int get_input_value_port_count() const;
	virtual int get_output_value_port_count() const;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const;

	virtual String get_caption() const;
	//virtual String get_text() const;
	virtual String get_category() const { return "functions"; }

	void set_func(BuiltinFunc p_which);
	BuiltinFunc get_func();

	virtual VisualScriptNodeInstance *instance(VisualScriptInstance *p_instance);

	VisualScriptBuiltinFunc(VisualScriptBuiltinFunc::BuiltinFunc func);
	VisualScriptBuiltinFunc();
};

VARIANT_ENUM_CAST(VisualScriptBuiltinFunc::BuiltinFunc)

void register_visual_script_builtin_func_node();

#endif // VISUAL_SCRIPT_BUILTIN_FUNCS_H
