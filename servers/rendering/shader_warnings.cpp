/**************************************************************************/
/*  shader_warnings.cpp                                                   */
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

#include "shader_warnings.h"
#include "core/variant/variant.h"

#ifdef DEBUG_ENABLED

ShaderWarning::Code ShaderWarning::get_code() const {
	return code;
}

int ShaderWarning::get_line() const {
	return line;
}

const StringName &ShaderWarning::get_subject() const {
	return subject;
}

String ShaderWarning::get_message() const {
	switch (code) {
		case FLOAT_COMPARISON:
			return vformat(RTR("Direct floating-point comparison (this may not evaluate to `true` as you expect). Instead, use `abs(a - b) < 0.0001` for an approximate but predictable comparison."));
		case UNUSED_CONSTANT:
			return vformat(RTR("The const '%s' is declared but never used."), subject);
		case UNUSED_FUNCTION:
			return vformat(RTR("The function '%s' is declared but never used."), subject);
		case UNUSED_STRUCT:
			return vformat(RTR("The struct '%s' is declared but never used."), subject);
		case UNUSED_UNIFORM:
			return vformat(RTR("The uniform '%s' is declared but never used."), subject);
		case UNUSED_VARYING:
			return vformat(RTR("The varying '%s' is declared but never used."), subject);
		case UNUSED_LOCAL_VARIABLE:
			return vformat(RTR("The local variable '%s' is declared but never used."), subject);
		case FORMATTING_ERROR:
			return subject;
		case DEVICE_LIMIT_EXCEEDED:
			return vformat(RTR("The total size of the %s for this shader on this device has been exceeded (%d/%d). The shader may not work correctly."), subject, (int)extra_args[0], (int)extra_args[1]);
		case MAGIC_POSITION_WRITE:
			return vformat(RTR("You are attempting to assign the VERTEX position in model space to the vertex POSITION in clip space. The definition of clip space changed in version 4.3, so if this code was written prior to 4.3, it will not continue to work. Consider specifying the clip space z-component directly i.e. use `vec4(VERTEX.xy, 1.0, 1.0)`."));
		default:
			break;
	}
	return String();
}

String ShaderWarning::get_name() const {
	return get_name_from_code(code);
}

Vector<Variant> ShaderWarning::get_extra_args() const {
	return extra_args;
}

String ShaderWarning::get_name_from_code(Code p_code) {
	ERR_FAIL_INDEX_V(p_code, WARNING_MAX, String());

	static const char *names[] = {
		"FLOAT_COMPARISON",
		"UNUSED_CONSTANT",
		"UNUSED_FUNCTION",
		"UNUSED_STRUCT",
		"UNUSED_UNIFORM",
		"UNUSED_VARYING",
		"UNUSED_LOCAL_VARIABLE",
		"FORMATTING_ERROR",
		"DEVICE_LIMIT_EXCEEDED",
		"MAGIC_POSITION_WRITE",
	};

	static_assert(std::size(names) == WARNING_MAX, "Amount of warning types don't match the amount of warning names.");

	return names[(int)p_code];
}

ShaderWarning::Code ShaderWarning::get_code_from_name(const String &p_name) {
	for (int i = 0; i < WARNING_MAX; i++) {
		if (get_name_from_code((Code)i) == p_name) {
			return (Code)i;
		}
	}

	ERR_FAIL_V_MSG(WARNING_MAX, "Invalid shader warning name: " + p_name);
}

static HashMap<int, uint32_t> *code_to_flags_map = nullptr;

static void init_code_to_flags_map() {
	code_to_flags_map = memnew((HashMap<int, uint32_t>));
	code_to_flags_map->insert(ShaderWarning::FLOAT_COMPARISON, ShaderWarning::FLOAT_COMPARISON_FLAG);
	code_to_flags_map->insert(ShaderWarning::UNUSED_CONSTANT, ShaderWarning::UNUSED_CONSTANT_FLAG);
	code_to_flags_map->insert(ShaderWarning::UNUSED_FUNCTION, ShaderWarning::UNUSED_FUNCTION_FLAG);
	code_to_flags_map->insert(ShaderWarning::UNUSED_STRUCT, ShaderWarning::UNUSED_STRUCT_FLAG);
	code_to_flags_map->insert(ShaderWarning::UNUSED_UNIFORM, ShaderWarning::UNUSED_UNIFORM_FLAG);
	code_to_flags_map->insert(ShaderWarning::UNUSED_VARYING, ShaderWarning::UNUSED_VARYING_FLAG);
	code_to_flags_map->insert(ShaderWarning::UNUSED_LOCAL_VARIABLE, ShaderWarning::UNUSED_LOCAL_VARIABLE_FLAG);
	code_to_flags_map->insert(ShaderWarning::FORMATTING_ERROR, ShaderWarning::FORMATTING_ERROR_FLAG);
	code_to_flags_map->insert(ShaderWarning::DEVICE_LIMIT_EXCEEDED, ShaderWarning::DEVICE_LIMIT_EXCEEDED_FLAG);
	code_to_flags_map->insert(ShaderWarning::MAGIC_POSITION_WRITE, ShaderWarning::MAGIC_POSITION_WRITE_FLAG);
}

ShaderWarning::CodeFlags ShaderWarning::get_flags_from_codemap(const HashMap<Code, bool> &p_map) {
	uint32_t result = 0U;

	if (code_to_flags_map == nullptr) {
		init_code_to_flags_map();
	}

	for (const KeyValue<Code, bool> &E : p_map) {
		if (E.value) {
			ERR_FAIL_COND_V(!code_to_flags_map->has((int)E.key), ShaderWarning::NONE_FLAG);
			result |= (*code_to_flags_map)[(int)E.key];
		}
	}
	return (CodeFlags)result;
}

ShaderWarning::ShaderWarning(Code p_code, int p_line, const StringName &p_subject, const Vector<Variant> &p_extra_args) :
		code(p_code), line(p_line), subject(p_subject), extra_args(p_extra_args) {
}

#endif // DEBUG_ENABLED
