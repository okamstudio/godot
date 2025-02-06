/**************************************************************************/
/*  call_error_info.cpp                                                   */
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

#include "call_error_info.h"

CallErrorInfo::CallError CallErrorInfo::get_call_error() {
	return call_error;
}

int CallErrorInfo::get_expected_arguments() {
	ERR_FAIL_COND_V_MSG(call_error != CALL_ERROR_TOO_MANY_ARGUMENTS && call_error != CALL_ERROR_TOO_FEW_ARGUMENTS, -1, "Error is not about expected argument count");
	return expected;
}

Variant::Type CallErrorInfo::get_invalid_argument_type() {
	ERR_FAIL_COND_V_MSG(call_error != CALL_ERROR_INVALID_ARGUMENT, Variant::NIL, "Error is not about an invalid argument");
	return Variant::Type(expected);
}
int CallErrorInfo::get_invalid_argument_index() {
	ERR_FAIL_COND_V_MSG(call_error != CALL_ERROR_INVALID_ARGUMENT, -1, "Error is not about an invalid argument");
	return argument;
}

void CallErrorInfo::set_call_error(CallError p_error, int p_argument, int p_expected) {
	ERR_FAIL_COND_MSG(p_error == CALL_ERROR_INVALID_ARGUMENT && (p_expected < 0 || expected >= Variant::VARIANT_MAX), "Invalid value for expected argument, must be a valid Variant type");
	call_error = p_error;
	argument = p_argument;
	expected = p_error;
}

void CallErrorInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_call_error"), &CallErrorInfo::get_call_error);
	ClassDB::bind_method(D_METHOD("get_expected_arguments"), &CallErrorInfo::get_expected_arguments);
	ClassDB::bind_method(D_METHOD("get_invalid_argument_type"), &CallErrorInfo::get_invalid_argument_type);
	ClassDB::bind_method(D_METHOD("get_invalid_argument_index"), &CallErrorInfo::get_invalid_argument_index);

	ClassDB::bind_method(D_METHOD("set_call_error", "error", "argument", "expected"), &CallErrorInfo::set_call_error);

	BIND_ENUM_CONSTANT(CALL_OK);
	BIND_ENUM_CONSTANT(CALL_ERROR_INVALID_METHOD);
	BIND_ENUM_CONSTANT(CALL_ERROR_INVALID_ARGUMENT);
	BIND_ENUM_CONSTANT(CALL_ERROR_TOO_MANY_ARGUMENTS);
	BIND_ENUM_CONSTANT(CALL_ERROR_TOO_FEW_ARGUMENTS);
	BIND_ENUM_CONSTANT(CALL_ERROR_INSTANCE_IS_NULL);
	BIND_ENUM_CONSTANT(CALL_ERROR_METHOD_NOT_CONST);
	BIND_ENUM_CONSTANT(CALL_ERROR_SCRIPT_ERROR);
}
