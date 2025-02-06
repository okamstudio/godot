/**************************************************************************/
/*  gdscript_disassembler.cpp                                             */
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

#ifdef DEBUG_ENABLED

#include "gdscript.h"
#include "gdscript_function.h"

#include "core/string/string_builder.h"

static String _get_variant_string(const Variant &p_variant) {
	String txt;
	if (p_variant.get_type() == Variant::STRING) {
		txt = "\"" + String(p_variant) + "\"";
	} else if (p_variant.get_type() == Variant::STRING_NAME) {
		txt = "&\"" + String(p_variant) + "\"";
	} else if (p_variant.get_type() == Variant::NODE_PATH) {
		txt = "^\"" + String(p_variant) + "\"";
	} else if (p_variant.get_type() == Variant::OBJECT) {
		Object *obj = p_variant;
		if (!obj) {
			txt = "null";
		} else {
			GDScriptNativeClass *cls = Object::cast_to<GDScriptNativeClass>(obj);
			if (cls) {
				txt = "class(" + cls->get_name() + ")";
			} else {
				Script *script = Object::cast_to<Script>(obj);
				if (script) {
					txt = "script(" + GDScript::debug_get_script_name(script) + ")";
				} else {
					txt = "object(" + obj->get_class();
					if (obj->get_script_instance()) {
						txt += ", " + GDScript::debug_get_script_name(obj->get_script_instance()->get_script());
					}
					txt += ")";
				}
			}
		}
	} else {
		txt = p_variant;
	}
	return txt;
}

static String _disassemble_address(const GDScript *p_script, const GDScriptFunction &p_function, int p_address) {
	int addr = p_address & GDScriptFunction::ADDR_MASK;

	switch (p_address >> GDScriptFunction::ADDR_BITS) {
		case GDScriptFunction::ADDR_TYPE_STACK: {
			switch (addr) {
				case GDScriptFunction::ADDR_STACK_SELF:
					return "self";
				case GDScriptFunction::ADDR_STACK_CLASS:
					return "class";
				case GDScriptFunction::ADDR_STACK_NIL:
					return "nil";
				default:
					return "stack(" + itos(addr) + ")";
			}
		} break;
		case GDScriptFunction::ADDR_TYPE_CONSTANT: {
			return "const(" + _get_variant_string(p_function.get_constant(addr)) + ")";
		} break;
		case GDScriptFunction::ADDR_TYPE_MEMBER: {
			return "member(" + p_script->debug_get_member_by_index(addr) + ")";
		} break;
	}

	return "<err>";
}

void GDScriptFunction::disassemble(const Vector<String> &p_code_lines) const {
#define DADDR(m_ip) (_disassemble_address(_script, *this, _code_ptr[ip + m_ip]))

	for (int ip = 0; ip < _code_size;) {
		StringBuilder text;
		int incr = 0;

		text += " ";
		text += itos(ip);
		text += ": ";

		// This makes the compiler complain if some opcode is unchecked in the switch.
		Opcode opcode = Opcode(_code_ptr[ip]);

		switch (opcode) {
			case OPCODE_OPERATOR: {
				constexpr int _pointer_size = sizeof(Variant::ValidatedOperatorEvaluator) / sizeof(*_code_ptr);
				int operation = _code_ptr[ip + 4];

				text += "operator ";

				text += DADDR(3);
				text += " = ";
				text += DADDR(1);
				text += " ";
				text += Variant::get_operator_name(Variant::Operator(operation));
				text += " ";
				text += DADDR(2);

				incr += 7 + _pointer_size;
			} break;
			case OPCODE_OPERATOR_VALIDATED: {
				text += "validated operator ";

				text += DADDR(3);
				text += " = ";
				text += DADDR(1);
				text += " ";
				text += operator_names[_code_ptr[ip + 4]];
				text += " ";
				text += DADDR(2);

				incr += 5;
			} break;
			case OPCODE_TYPE_TEST_BUILTIN: {
				text += "type test ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);
				text += " is ";
				text += Variant::get_type_name(Variant::Type(_code_ptr[ip + 3]));

				incr += 4;
			} break;
			case OPCODE_TYPE_TEST_ARRAY: {
				text += "type test ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);
				text += " is Array[";

				Ref<Script> script_type = get_constant(_code_ptr[ip + 3] & ADDR_MASK);
				Variant::Type builtin_type = (Variant::Type)_code_ptr[ip + 4];
				StringName native_type = get_global_name(_code_ptr[ip + 5]);

				if (script_type.is_valid() && script_type->is_valid()) {
					text += "script(";
					text += GDScript::debug_get_script_name(script_type);
					text += ")";
				} else if (native_type != StringName()) {
					text += native_type;
				} else {
					text += Variant::get_type_name(builtin_type);
				}

				text += "]";

				incr += 6;
			} break;
			case OPCODE_TYPE_TEST_DICTIONARY: {
				text += "type test ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);
				text += " is Dictionary[";

				Ref<Script> key_script_type = get_constant(_code_ptr[ip + 3] & ADDR_MASK);
				Variant::Type key_builtin_type = (Variant::Type)_code_ptr[ip + 5];
				StringName key_native_type = get_global_name(_code_ptr[ip + 6]);

				if (key_script_type.is_valid() && key_script_type->is_valid()) {
					text += "script(";
					text += GDScript::debug_get_script_name(key_script_type);
					text += ")";
				} else if (key_native_type != StringName()) {
					text += key_native_type;
				} else {
					text += Variant::get_type_name(key_builtin_type);
				}

				text += ", ";

				Ref<Script> value_script_type = get_constant(_code_ptr[ip + 4] & ADDR_MASK);
				Variant::Type value_builtin_type = (Variant::Type)_code_ptr[ip + 7];
				StringName value_native_type = get_global_name(_code_ptr[ip + 8]);

				if (value_script_type.is_valid() && value_script_type->is_valid()) {
					text += "script(";
					text += GDScript::debug_get_script_name(value_script_type);
					text += ")";
				} else if (value_native_type != StringName()) {
					text += value_native_type;
				} else {
					text += Variant::get_type_name(value_builtin_type);
				}

				text += "]";

				incr += 9;
			} break;
			case OPCODE_TYPE_TEST_NATIVE: {
				text += "type test ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);
				text += " is ";
				text += get_global_name(_code_ptr[ip + 3]);

				incr += 4;
			} break;
			case OPCODE_TYPE_TEST_SCRIPT: {
				text += "type test ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);
				text += " is ";
				text += DADDR(3);

				incr += 4;
			} break;
			case OPCODE_SET_KEYED: {
				text += "set keyed ";
				text += DADDR(1);
				text += "[";
				text += DADDR(2);
				text += "] = ";
				text += DADDR(3);

				incr += 4;
			} break;
			case OPCODE_SET_KEYED_VALIDATED: {
				text += "set keyed validated ";
				text += DADDR(1);
				text += "[";
				text += DADDR(2);
				text += "] = ";
				text += DADDR(3);

				incr += 5;
			} break;
			case OPCODE_SET_INDEXED_VALIDATED: {
				text += "set indexed validated ";
				text += DADDR(1);
				text += "[";
				text += DADDR(2);
				text += "] = ";
				text += DADDR(3);

				incr += 5;
			} break;
			case OPCODE_GET_KEYED: {
				text += "get keyed ";
				text += DADDR(3);
				text += " = ";
				text += DADDR(1);
				text += "[";
				text += DADDR(2);
				text += "]";

				incr += 4;
			} break;
			case OPCODE_GET_KEYED_VALIDATED: {
				text += "get keyed validated ";
				text += DADDR(3);
				text += " = ";
				text += DADDR(1);
				text += "[";
				text += DADDR(2);
				text += "]";

				incr += 5;
			} break;
			case OPCODE_GET_INDEXED_VALIDATED: {
				text += "get indexed validated ";
				text += DADDR(3);
				text += " = ";
				text += DADDR(1);
				text += "[";
				text += DADDR(2);
				text += "]";

				incr += 5;
			} break;
			case OPCODE_SET_NAMED: {
				text += "set_named ";
				text += DADDR(1);
				text += "[\"";
				text += _global_names_ptr[_code_ptr[ip + 3]];
				text += "\"] = ";
				text += DADDR(2);

				incr += 4;
			} break;
			case OPCODE_SET_NAMED_VALIDATED: {
				text += "set_named validated ";
				text += DADDR(1);
				text += "[\"";
				text += setter_names[_code_ptr[ip + 3]];
				text += "\"] = ";
				text += DADDR(2);

				incr += 4;
			} break;
			case OPCODE_GET_NAMED: {
				text += "get_named ";
				text += DADDR(2);
				text += " = ";
				text += DADDR(1);
				text += "[\"";
				text += _global_names_ptr[_code_ptr[ip + 3]];
				text += "\"]";

				incr += 4;
			} break;
			case OPCODE_GET_NAMED_VALIDATED: {
				text += "get_named validated ";
				text += DADDR(2);
				text += " = ";
				text += DADDR(1);
				text += "[\"";
				text += getter_names[_code_ptr[ip + 3]];
				text += "\"]";

				incr += 4;
			} break;
			case OPCODE_SET_MEMBER: {
				text += "set_member ";
				text += "[\"";
				text += _global_names_ptr[_code_ptr[ip + 2]];
				text += "\"] = ";
				text += DADDR(1);

				incr += 3;
			} break;
			case OPCODE_GET_MEMBER: {
				text += "get_member ";
				text += DADDR(1);
				text += " = ";
				text += "[\"";
				text += _global_names_ptr[_code_ptr[ip + 2]];
				text += "\"]";

				incr += 3;
			} break;
			case OPCODE_SET_STATIC_VARIABLE: {
				Ref<GDScript> gdscript;
				if (_code_ptr[ip + 2] == ADDR_CLASS) {
					gdscript = Ref<GDScript>(_script);
				} else {
					gdscript = get_constant(_code_ptr[ip + 2] & ADDR_MASK);
				}

				text += "set_static_variable script(";
				text += GDScript::debug_get_script_name(gdscript);
				text += ")";
				if (gdscript.is_valid()) {
					text += "[\"" + gdscript->debug_get_static_var_by_index(_code_ptr[ip + 3]) + "\"]";
				} else {
					text += "[<index " + itos(_code_ptr[ip + 3]) + ">]";
				}
				text += " = ";
				text += DADDR(1);

				incr += 4;
			} break;
			case OPCODE_GET_STATIC_VARIABLE: {
				Ref<GDScript> gdscript;
				if (_code_ptr[ip + 2] == ADDR_CLASS) {
					gdscript = Ref<GDScript>(_script);
				} else {
					gdscript = get_constant(_code_ptr[ip + 2] & ADDR_MASK);
				}

				text += "get_static_variable ";
				text += DADDR(1);
				text += " = script(";
				text += GDScript::debug_get_script_name(gdscript);
				text += ")";
				if (gdscript.is_valid()) {
					text += "[\"" + gdscript->debug_get_static_var_by_index(_code_ptr[ip + 3]) + "\"]";
				} else {
					text += "[<index " + itos(_code_ptr[ip + 3]) + ">]";
				}

				incr += 4;
			} break;
			case OPCODE_ASSIGN: {
				text += "assign ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);

				incr += 3;
			} break;
			case OPCODE_ASSIGN_NULL: {
				text += "assign ";
				text += DADDR(1);
				text += " = null";

				incr += 2;
			} break;
			case OPCODE_ASSIGN_TRUE: {
				text += "assign ";
				text += DADDR(1);
				text += " = true";

				incr += 2;
			} break;
			case OPCODE_ASSIGN_FALSE: {
				text += "assign ";
				text += DADDR(1);
				text += " = false";

				incr += 2;
			} break;
			case OPCODE_ASSIGN_TYPED_BUILTIN: {
				text += "assign typed builtin (";
				text += Variant::get_type_name((Variant::Type)_code_ptr[ip + 3]);
				text += ") ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);

				incr += 4;
			} break;
			case OPCODE_ASSIGN_TYPED_ARRAY: {
				text += "assign typed array ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);

				incr += 6;
			} break;
			case OPCODE_ASSIGN_TYPED_DICTIONARY: {
				text += "assign typed dictionary ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);

				incr += 9;
			} break;
			case OPCODE_ASSIGN_TYPED_NATIVE: {
				text += "assign typed native (";
				text += DADDR(3);
				text += ") ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);

				incr += 4;
			} break;
			case OPCODE_ASSIGN_TYPED_SCRIPT: {
				Ref<Script> script = get_constant(_code_ptr[ip + 3] & ADDR_MASK);

				text += "assign typed script (";
				text += GDScript::debug_get_script_name(script);
				text += ") ";
				text += DADDR(1);
				text += " = ";
				text += DADDR(2);

				incr += 4;
			} break;
			case OPCODE_CAST_TO_BUILTIN: {
				text += "cast builtin ";
				text += DADDR(2);
				text += " = ";
				text += DADDR(1);
				text += " as ";
				text += Variant::get_type_name(Variant::Type(_code_ptr[ip + 1]));

				incr += 4;
			} break;
			case OPCODE_CAST_TO_NATIVE: {
				text += "cast native ";
				text += DADDR(2);
				text += " = ";
				text += DADDR(1);
				text += " as ";
				text += DADDR(3);

				incr += 4;
			} break;
			case OPCODE_CAST_TO_SCRIPT: {
				text += "cast ";
				text += DADDR(2);
				text += " = ";
				text += DADDR(1);
				text += " as ";
				text += DADDR(3);

				incr += 4;
			} break;
			case OPCODE_CONSTRUCT: {
				int instr_var_args = _code_ptr[++ip];
				Variant::Type t = Variant::Type(_code_ptr[ip + 3 + instr_var_args]);
				int argc = _code_ptr[ip + 1 + instr_var_args];

				text += "construct ";
				text += DADDR(1 + argc);
				text += " = ";

				text += Variant::get_type_name(t) + "(";
				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(i + 1);
				}
				text += ")";

				incr = 3 + instr_var_args;
			} break;
			case OPCODE_CONSTRUCT_VALIDATED: {
				int instr_var_args = _code_ptr[++ip];
				int argc = _code_ptr[ip + 1 + instr_var_args];

				text += "construct validated ";
				text += DADDR(1 + argc);
				text += " = ";

				text += constructors_names[_code_ptr[ip + 3 + argc]];
				text += "(";
				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(i + 1);
				}
				text += ")";

				incr = 3 + instr_var_args;
			} break;
			case OPCODE_CONSTRUCT_ARRAY: {
				int instr_var_args = _code_ptr[++ip];
				int argc = _code_ptr[ip + 1 + instr_var_args];
				text += "make_array ";
				text += DADDR(1 + argc);
				text += " = [";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}

				text += "]";

				incr += 3 + argc;
			} break;
			case OPCODE_CONSTRUCT_TYPED_ARRAY: {
				int instr_var_args = _code_ptr[++ip];
				int argc = _code_ptr[ip + 1 + instr_var_args];

				Ref<Script> script_type = get_constant(_code_ptr[ip + argc + 2] & ADDR_MASK);
				Variant::Type builtin_type = (Variant::Type)_code_ptr[ip + argc + 4];
				StringName native_type = get_global_name(_code_ptr[ip + argc + 5]);

				String type_name;
				if (script_type.is_valid() && script_type->is_valid()) {
					type_name = "script(" + GDScript::debug_get_script_name(script_type) + ")";
				} else if (native_type != StringName()) {
					type_name = native_type;
				} else {
					type_name = Variant::get_type_name(builtin_type);
				}

				text += "make_typed_array (";
				text += type_name;
				text += ") ";

				text += DADDR(1 + argc);
				text += " = [";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}

				text += "]";

				incr += 6 + argc;
			} break;
			case OPCODE_CONSTRUCT_DICTIONARY: {
				int instr_var_args = _code_ptr[++ip];
				int argc = _code_ptr[ip + 1 + instr_var_args];
				text += "make_dict ";
				text += DADDR(1 + argc * 2);
				text += " = {";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i * 2 + 0);
					text += ": ";
					text += DADDR(1 + i * 2 + 1);
				}

				text += "}";

				incr += 3 + argc * 2;
			} break;
			case OPCODE_CONSTRUCT_TYPED_DICTIONARY: {
				int instr_var_args = _code_ptr[++ip];
				int argc = _code_ptr[ip + 1 + instr_var_args];

				Ref<Script> key_script_type = get_constant(_code_ptr[ip + argc * 2 + 2] & ADDR_MASK);
				Variant::Type key_builtin_type = (Variant::Type)_code_ptr[ip + argc * 2 + 5];
				StringName key_native_type = get_global_name(_code_ptr[ip + argc * 2 + 6]);

				String key_type_name;
				if (key_script_type.is_valid() && key_script_type->is_valid()) {
					key_type_name = "script(" + GDScript::debug_get_script_name(key_script_type) + ")";
				} else if (key_native_type != StringName()) {
					key_type_name = key_native_type;
				} else {
					key_type_name = Variant::get_type_name(key_builtin_type);
				}

				Ref<Script> value_script_type = get_constant(_code_ptr[ip + argc * 2 + 3] & ADDR_MASK);
				Variant::Type value_builtin_type = (Variant::Type)_code_ptr[ip + argc * 2 + 7];
				StringName value_native_type = get_global_name(_code_ptr[ip + argc * 2 + 8]);

				String value_type_name;
				if (value_script_type.is_valid() && value_script_type->is_valid()) {
					value_type_name = "script(" + GDScript::debug_get_script_name(value_script_type) + ")";
				} else if (value_native_type != StringName()) {
					value_type_name = value_native_type;
				} else {
					value_type_name = Variant::get_type_name(value_builtin_type);
				}

				text += "make_typed_dict (";
				text += key_type_name;
				text += ", ";
				text += value_type_name;
				text += ") ";

				text += DADDR(1 + argc * 2);
				text += " = {";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i * 2 + 0);
					text += ": ";
					text += DADDR(1 + i * 2 + 1);
				}

				text += "}";

				incr += 9 + argc * 2;
			} break;
			case OPCODE_CALL:
			case OPCODE_CALL_RETURN:
			case OPCODE_CALL_ASYNC: {
				bool ret = (_code_ptr[ip]) == OPCODE_CALL_RETURN;
				bool async = (_code_ptr[ip]) == OPCODE_CALL_ASYNC;

				int instr_var_args = _code_ptr[++ip];

				if (ret) {
					text += "call-ret ";
				} else if (async) {
					text += "call-async ";
				} else {
					text += "call ";
				}

				int argc = _code_ptr[ip + 1 + instr_var_args];
				if (ret || async) {
					text += DADDR(2 + argc) + " = ";
				}

				text += DADDR(1 + argc) + ".";
				text += String(_global_names_ptr[_code_ptr[ip + 2 + instr_var_args]]);
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 5 + argc;
			} break;
			case OPCODE_CALL_METHOD_BIND:
			case OPCODE_CALL_METHOD_BIND_RET: {
				bool ret = (_code_ptr[ip]) == OPCODE_CALL_METHOD_BIND_RET;
				int instr_var_args = _code_ptr[++ip];

				if (ret) {
					text += "call-method_bind-ret ";
				} else {
					text += "call-method_bind ";
				}

				MethodBind *method = _methods_ptr[_code_ptr[ip + 2 + instr_var_args]];

				int argc = _code_ptr[ip + 1 + instr_var_args];
				if (ret) {
					text += DADDR(2 + argc) + " = ";
				}

				text += DADDR(1 + argc) + ".";
				text += method->get_name();
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 5 + argc;
			} break;
			case OPCODE_CALL_BUILTIN_STATIC: {
				int instr_var_args = _code_ptr[++ip];
				Variant::Type type = (Variant::Type)_code_ptr[ip + 1 + instr_var_args];
				int argc = _code_ptr[ip + 3 + instr_var_args];

				text += "call built-in method static ";
				text += DADDR(1 + argc);
				text += " = ";
				text += Variant::get_type_name(type);
				text += ".";
				text += _global_names_ptr[_code_ptr[ip + 2 + instr_var_args]].operator String();
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr += 5 + argc;
			} break;
			case OPCODE_CALL_NATIVE_STATIC: {
				int instr_var_args = _code_ptr[++ip];
				MethodBind *method = _methods_ptr[_code_ptr[ip + 1 + instr_var_args]];
				int argc = _code_ptr[ip + 2 + instr_var_args];

				text += "call native method static ";
				text += DADDR(1 + argc);
				text += " = ";
				text += method->get_instance_class();
				text += ".";
				text += method->get_name();
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr += 4 + argc;
			} break;

			case OPCODE_CALL_NATIVE_STATIC_VALIDATED_RETURN: {
				int instr_var_args = _code_ptr[++ip];
				text += "call native static method validated (return) ";
				MethodBind *method = _methods_ptr[_code_ptr[ip + 2 + instr_var_args]];
				int argc = _code_ptr[ip + 1 + instr_var_args];
				text += DADDR(1 + argc) + " = ";
				text += method->get_instance_class();
				text += ".";
				text += method->get_name();
				text += "(";
				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";
				incr = 4 + argc;
			} break;

			case OPCODE_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN: {
				int instr_var_args = _code_ptr[++ip];

				text += "call native static method validated (no return) ";

				MethodBind *method = _methods_ptr[_code_ptr[ip + 2 + instr_var_args]];

				int argc = _code_ptr[ip + 1 + instr_var_args];

				text += method->get_instance_class();
				text += ".";
				text += method->get_name();
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 4 + argc;
			} break;

			case OPCODE_CALL_METHOD_BIND_VALIDATED_RETURN: {
				int instr_var_args = _code_ptr[++ip];
				text += "call method-bind validated (return) ";
				MethodBind *method = _methods_ptr[_code_ptr[ip + 2 + instr_var_args]];
				int argc = _code_ptr[ip + 1 + instr_var_args];
				text += DADDR(2 + argc) + " = ";
				text += DADDR(1 + argc) + ".";
				text += method->get_name();
				text += "(";
				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";
				incr = 5 + argc;
			} break;

			case OPCODE_CALL_METHOD_BIND_VALIDATED_NO_RETURN: {
				int instr_var_args = _code_ptr[++ip];

				text += "call method-bind validated (no return) ";

				MethodBind *method = _methods_ptr[_code_ptr[ip + 2 + instr_var_args]];

				int argc = _code_ptr[ip + 1 + instr_var_args];

				text += DADDR(1 + argc) + ".";
				text += method->get_name();
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 5 + argc;
			} break;

			case OPCODE_CALL_BUILTIN_TYPE_VALIDATED: {
				int instr_var_args = _code_ptr[++ip];
				int argc = _code_ptr[ip + 1 + instr_var_args];

				text += "call-builtin-method validated ";

				text += DADDR(2 + argc) + " = ";

				text += DADDR(1) + ".";
				text += builtin_methods_names[_code_ptr[ip + 4 + argc]];

				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 5 + argc;
			} break;
			case OPCODE_CALL_UTILITY: {
				int instr_var_args = _code_ptr[++ip];

				text += "call-utility ";

				int argc = _code_ptr[ip + 1 + instr_var_args];
				text += DADDR(1 + argc) + " = ";

				text += _global_names_ptr[_code_ptr[ip + 2 + instr_var_args]];
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 4 + argc;
			} break;
			case OPCODE_CALL_UTILITY_VALIDATED: {
				int instr_var_args = _code_ptr[++ip];

				text += "call-utility validated ";

				int argc = _code_ptr[ip + 1 + instr_var_args];
				text += DADDR(1 + argc) + " = ";

				text += utilities_names[_code_ptr[ip + 3 + argc]];
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 4 + argc;
			} break;
			case OPCODE_CALL_GDSCRIPT_UTILITY: {
				int instr_var_args = _code_ptr[++ip];

				text += "call-gdscript-utility ";

				int argc = _code_ptr[ip + 1 + instr_var_args];
				text += DADDR(1 + argc) + " = ";

				text += gds_utilities_names[_code_ptr[ip + 3 + argc]];
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 4 + argc;
			} break;
			case OPCODE_CALL_SELF_BASE: {
				int instr_var_args = _code_ptr[++ip];

				text += "call-self-base ";

				int argc = _code_ptr[ip + 1 + instr_var_args];
				text += DADDR(2 + argc) + " = ";

				text += _global_names_ptr[_code_ptr[ip + 2 + instr_var_args]];
				text += "(";

				for (int i = 0; i < argc; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 4 + argc;
			} break;
			case OPCODE_AWAIT: {
				text += "await ";
				text += DADDR(1);

				incr = 2;
			} break;
			case OPCODE_AWAIT_RESUME: {
				text += "await resume ";
				text += DADDR(1);

				incr = 2;
			} break;
			case OPCODE_CREATE_LAMBDA: {
				int instr_var_args = _code_ptr[++ip];
				int captures_count = _code_ptr[ip + 1 + instr_var_args];
				GDScriptFunction *lambda = _lambdas_ptr[_code_ptr[ip + 2 + instr_var_args]];

				text += DADDR(1 + captures_count);
				text += "create lambda from ";
				text += lambda->name.operator String();
				text += "function, captures (";

				for (int i = 0; i < captures_count; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 4 + captures_count;
			} break;
			case OPCODE_CREATE_SELF_LAMBDA: {
				int instr_var_args = _code_ptr[++ip];
				int captures_count = _code_ptr[ip + 1 + instr_var_args];
				GDScriptFunction *lambda = _lambdas_ptr[_code_ptr[ip + 2 + instr_var_args]];

				text += DADDR(1 + captures_count);
				text += "create self lambda from ";
				text += lambda->name.operator String();
				text += "function, captures (";

				for (int i = 0; i < captures_count; i++) {
					if (i > 0) {
						text += ", ";
					}
					text += DADDR(1 + i);
				}
				text += ")";

				incr = 4 + captures_count;
			} break;
			case OPCODE_JUMP: {
				text += "jump ";
				text += itos(_code_ptr[ip + 1]);

				incr = 2;
			} break;
			case OPCODE_JUMP_IF: {
				text += "jump-if ";
				text += DADDR(1);
				text += " to ";
				text += itos(_code_ptr[ip + 2]);

				incr = 3;
			} break;
			case OPCODE_JUMP_IF_NOT: {
				text += "jump-if-not ";
				text += DADDR(1);
				text += " to ";
				text += itos(_code_ptr[ip + 2]);

				incr = 3;
			} break;
			case OPCODE_JUMP_TO_DEF_ARGUMENT: {
				text += "jump-to-default-argument ";

				incr = 1;
			} break;
			case OPCODE_JUMP_IF_SHARED: {
				text += "jump-if-shared ";
				text += DADDR(1);
				text += " to ";
				text += itos(_code_ptr[ip + 2]);

				incr = 3;
			} break;
			case OPCODE_RETURN: {
				text += "return ";
				text += DADDR(1);

				incr = 2;
			} break;
			case OPCODE_RETURN_TYPED_BUILTIN: {
				text += "return typed builtin (";
				text += Variant::get_type_name((Variant::Type)_code_ptr[ip + 2]);
				text += ") ";
				text += DADDR(1);

				incr += 3;
			} break;
			case OPCODE_RETURN_TYPED_ARRAY: {
				text += "return typed array ";
				text += DADDR(1);

				incr += 5;
			} break;
			case OPCODE_RETURN_TYPED_DICTIONARY: {
				text += "return typed dictionary ";
				text += DADDR(1);

				incr += 8;
			} break;
			case OPCODE_RETURN_TYPED_NATIVE: {
				text += "return typed native (";
				text += DADDR(2);
				text += ") ";
				text += DADDR(1);

				incr += 3;
			} break;
			case OPCODE_RETURN_TYPED_SCRIPT: {
				Ref<Script> script = get_constant(_code_ptr[ip + 2] & ADDR_MASK);

				text += "return typed script (";
				text += GDScript::debug_get_script_name(script);
				text += ") ";
				text += DADDR(1);

				incr += 3;
			} break;

#define DISASSEMBLE_ITERATE(m_type)      \
	case OPCODE_ITERATE_##m_type: {      \
		text += "for-loop (typed ";      \
		text += #m_type;                 \
		text += ") ";                    \
		text += DADDR(3);                \
		text += " in ";                  \
		text += DADDR(2);                \
		text += " counter ";             \
		text += DADDR(1);                \
		text += " end ";                 \
		text += itos(_code_ptr[ip + 4]); \
		incr += 5;                       \
	} break

#define DISASSEMBLE_ITERATE_BEGIN(m_type) \
	case OPCODE_ITERATE_BEGIN_##m_type: { \
		text += "for-init (typed ";       \
		text += #m_type;                  \
		text += ") ";                     \
		text += DADDR(3);                 \
		text += " in ";                   \
		text += DADDR(2);                 \
		text += " counter ";              \
		text += DADDR(1);                 \
		text += " end ";                  \
		text += itos(_code_ptr[ip + 4]);  \
		incr += 5;                        \
	} break

#define DISASSEMBLE_ITERATE_TYPES(m_macro) \
	m_macro(INT);                          \
	m_macro(FLOAT);                        \
	m_macro(VECTOR2);                      \
	m_macro(VECTOR2I);                     \
	m_macro(VECTOR3);                      \
	m_macro(VECTOR3I);                     \
	m_macro(STRING);                       \
	m_macro(DICTIONARY);                   \
	m_macro(ARRAY);                        \
	m_macro(PACKED_BYTE_ARRAY);            \
	m_macro(PACKED_INT32_ARRAY);           \
	m_macro(PACKED_INT64_ARRAY);           \
	m_macro(PACKED_FLOAT32_ARRAY);         \
	m_macro(PACKED_FLOAT64_ARRAY);         \
	m_macro(PACKED_STRING_ARRAY);          \
	m_macro(PACKED_VECTOR2_ARRAY);         \
	m_macro(PACKED_VECTOR3_ARRAY);         \
	m_macro(PACKED_COLOR_ARRAY);           \
	m_macro(PACKED_VECTOR4_ARRAY);         \
	m_macro(OBJECT)

			case OPCODE_ITERATE_BEGIN: {
				text += "for-init ";
				text += DADDR(3);
				text += " in ";
				text += DADDR(2);
				text += " counter ";
				text += DADDR(1);
				text += " end ";
				text += itos(_code_ptr[ip + 4]);

				incr += 5;
			} break;
				DISASSEMBLE_ITERATE_TYPES(DISASSEMBLE_ITERATE_BEGIN);
			case OPCODE_ITERATE_BEGIN_RANGE: {
				text += "for-init ";
				text += DADDR(5);
				text += " in range from ";
				text += DADDR(2);
				text += " to ";
				text += DADDR(3);
				text += " step ";
				text += DADDR(4);
				text += " counter ";
				text += DADDR(1);
				text += " end ";
				text += itos(_code_ptr[ip + 6]);

				incr += 7;
			} break;
			case OPCODE_ITERATE: {
				text += "for-loop ";
				text += DADDR(3);
				text += " in ";
				text += DADDR(2);
				text += " counter ";
				text += DADDR(1);
				text += " end ";
				text += itos(_code_ptr[ip + 4]);

				incr += 5;
			} break;
				DISASSEMBLE_ITERATE_TYPES(DISASSEMBLE_ITERATE);
			case OPCODE_ITERATE_RANGE: {
				text += "for-loop ";
				text += DADDR(4);
				text += " in range to ";
				text += DADDR(2);
				text += " step ";
				text += DADDR(3);
				text += " counter ";
				text += DADDR(1);
				text += " end ";
				text += itos(_code_ptr[ip + 5]);

				incr += 6;
			} break;
			case OPCODE_STORE_GLOBAL: {
				text += "store global ";
				text += DADDR(1);
				text += " = ";
				text += String::num_int64(_code_ptr[ip + 2]);

				incr += 3;
			} break;
			case OPCODE_STORE_NAMED_GLOBAL: {
				text += "store named global ";
				text += DADDR(1);
				text += " = ";
				text += String(_global_names_ptr[_code_ptr[ip + 2]]);

				incr += 3;
			} break;
			case OPCODE_LINE: {
				int line = _code_ptr[ip + 1] - 1;
				if (line >= 0 && line < p_code_lines.size()) {
					text += "line ";
					text += itos(line + 1);
					text += ": ";
					text += p_code_lines[line];
				} else {
					text += "";
				}

				incr += 2;
			} break;

#define DISASSEMBLE_TYPE_ADJUST(m_v_type) \
	case OPCODE_TYPE_ADJUST_##m_v_type: { \
		text += "type adjust (";          \
		text += #m_v_type;                \
		text += ") ";                     \
		text += DADDR(1);                 \
		incr += 2;                        \
	} break

				DISASSEMBLE_TYPE_ADJUST(BOOL);
				DISASSEMBLE_TYPE_ADJUST(INT);
				DISASSEMBLE_TYPE_ADJUST(FLOAT);
				DISASSEMBLE_TYPE_ADJUST(STRING);
				DISASSEMBLE_TYPE_ADJUST(VECTOR2);
				DISASSEMBLE_TYPE_ADJUST(VECTOR2I);
				DISASSEMBLE_TYPE_ADJUST(RECT2);
				DISASSEMBLE_TYPE_ADJUST(RECT2I);
				DISASSEMBLE_TYPE_ADJUST(VECTOR3);
				DISASSEMBLE_TYPE_ADJUST(VECTOR3I);
				DISASSEMBLE_TYPE_ADJUST(TRANSFORM2D);
				DISASSEMBLE_TYPE_ADJUST(VECTOR4);
				DISASSEMBLE_TYPE_ADJUST(VECTOR4I);
				DISASSEMBLE_TYPE_ADJUST(PLANE);
				DISASSEMBLE_TYPE_ADJUST(QUATERNION);
				DISASSEMBLE_TYPE_ADJUST(AABB);
				DISASSEMBLE_TYPE_ADJUST(BASIS);
				DISASSEMBLE_TYPE_ADJUST(TRANSFORM3D);
				DISASSEMBLE_TYPE_ADJUST(PROJECTION);
				DISASSEMBLE_TYPE_ADJUST(COLOR);
				DISASSEMBLE_TYPE_ADJUST(STRING_NAME);
				DISASSEMBLE_TYPE_ADJUST(NODE_PATH);
				DISASSEMBLE_TYPE_ADJUST(RID);
				DISASSEMBLE_TYPE_ADJUST(OBJECT);
				DISASSEMBLE_TYPE_ADJUST(CALLABLE);
				DISASSEMBLE_TYPE_ADJUST(SIGNAL);
				DISASSEMBLE_TYPE_ADJUST(DICTIONARY);
				DISASSEMBLE_TYPE_ADJUST(ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_BYTE_ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_INT32_ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_INT64_ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_FLOAT32_ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_FLOAT64_ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_STRING_ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_VECTOR2_ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_VECTOR3_ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_COLOR_ARRAY);
				DISASSEMBLE_TYPE_ADJUST(PACKED_VECTOR4_ARRAY);

			case OPCODE_ASSERT: {
				text += "assert (";
				text += DADDR(1);
				text += ", ";
				text += DADDR(2);
				text += ")";

				incr += 3;
			} break;
			case OPCODE_BREAKPOINT: {
				text += "breakpoint";

				incr += 1;
			} break;
			case OPCODE_END: {
				text += "== END ==";

				incr += 1;
			} break;
		}

		ip += incr;
		if (text.get_string_length() > 0) {
			print_line(text.as_string());
		}
	}
}

#endif // DEBUG_ENABLED
