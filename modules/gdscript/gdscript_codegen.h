/**************************************************************************/
/*  gdscript_codegen.h                                                    */
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

#include "gdscript_function.h"
#include "gdscript_utility_functions.h"

#include "core/string/string_name.h"
#include "core/variant/variant.h"

class GDScriptCodeGenerator {
public:
	struct Address {
		enum AddressMode {
			SELF,
			CLASS,
			MEMBER,
			CONSTANT,
			LOCAL_VARIABLE,
			FUNCTION_PARAMETER,
			TEMPORARY,
			NIL,
		};
		AddressMode mode = NIL;
		uint32_t address = 0;
		GDScriptDataType type;

		Address() {}
		Address(AddressMode p_mode, const GDScriptDataType &p_type = GDScriptDataType()) {
			mode = p_mode;
			type = p_type;
		}
		Address(AddressMode p_mode, uint32_t p_address, const GDScriptDataType &p_type = GDScriptDataType()) {
			mode = p_mode;
			address = p_address;
			type = p_type;
		}
	};

	virtual uint32_t add_parameter(const StringName &p_name, bool p_is_optional, const GDScriptDataType &p_type) = 0;
	virtual uint32_t add_local(const StringName &p_name, const GDScriptDataType &p_type) = 0;
	virtual uint32_t add_local_constant(const StringName &p_name, const Variant &p_constant) = 0;
	virtual uint32_t add_or_get_constant(const Variant &p_constant) = 0;
	virtual uint32_t add_or_get_name(const StringName &p_name) = 0;
	virtual uint32_t add_temporary(const GDScriptDataType &p_type) = 0;
	virtual void pop_temporary() = 0;
	virtual void clear_temporaries() = 0;
	virtual void clear_address(const Address &p_address) = 0;
	virtual bool is_local_dirty(const Address &p_address) const = 0;

	virtual void start_parameters() = 0;
	virtual void end_parameters() = 0;

	virtual void start_block() = 0;
	virtual void end_block() = 0;

	virtual void write_start(GDScript *p_script, const StringName &p_function_name, bool p_static, Variant p_rpc_config, const GDScriptDataType &p_return_type) = 0;
	virtual GDScriptFunction *write_end() = 0;

#ifdef DEBUG_ENABLED
	virtual void set_signature(const String &p_signature) = 0;
#endif
	virtual void set_initial_line(int p_line) = 0;

	virtual void write_type_adjust(const Address &p_target, Variant::Type p_new_type) = 0;
	virtual void write_unary_operator(const Address &p_target, Variant::Operator p_operator, const Address &p_left_operand) = 0;
	virtual void write_binary_operator(const Address &p_target, Variant::Operator p_operator, const Address &p_left_operand, const Address &p_right_operand) = 0;
	virtual void write_type_test(const Address &p_target, const Address &p_source, const GDScriptDataType &p_type) = 0;
	virtual void write_and_left_operand(const Address &p_left_operand) = 0;
	virtual void write_and_right_operand(const Address &p_right_operand) = 0;
	virtual void write_end_and(const Address &p_target) = 0;
	virtual void write_or_left_operand(const Address &p_left_operand) = 0;
	virtual void write_or_right_operand(const Address &p_right_operand) = 0;
	virtual void write_end_or(const Address &p_target) = 0;
	virtual void write_start_ternary(const Address &p_target) = 0;
	virtual void write_ternary_condition(const Address &p_condition) = 0;
	virtual void write_ternary_true_expr(const Address &p_expr) = 0;
	virtual void write_ternary_false_expr(const Address &p_expr) = 0;
	virtual void write_end_ternary() = 0;
	virtual void write_set(const Address &p_target, const Address &p_index, const Address &p_source) = 0;
	virtual void write_get(const Address &p_target, const Address &p_index, const Address &p_source) = 0;
	virtual void write_set_named(const Address &p_target, const StringName &p_name, const Address &p_source) = 0;
	virtual void write_get_named(const Address &p_target, const StringName &p_name, const Address &p_source) = 0;
	virtual void write_set_member(const Address &p_value, const StringName &p_name) = 0;
	virtual void write_get_member(const Address &p_target, const StringName &p_name) = 0;
	virtual void write_set_static_variable(const Address &p_value, const Address &p_class, int p_index) = 0;
	virtual void write_get_static_variable(const Address &p_target, const Address &p_class, int p_index) = 0;
	virtual void write_assign(const Address &p_target, const Address &p_source) = 0;
	virtual void write_assign_with_conversion(const Address &p_target, const Address &p_source) = 0;
	virtual void write_assign_null(const Address &p_target) = 0;
	virtual void write_assign_true(const Address &p_target) = 0;
	virtual void write_assign_false(const Address &p_target) = 0;
	virtual void write_assign_default_parameter(const Address &dst, const Address &src, bool p_use_conversion) = 0;
	virtual void write_store_global(const Address &p_dst, int p_global_index) = 0;
	virtual void write_store_named_global(const Address &p_dst, const StringName &p_global) = 0;
	virtual void write_cast(const Address &p_target, const Address &p_source, const GDScriptDataType &p_type) = 0;
	virtual void write_call(const Address &p_target, const Address &p_base, const StringName &p_function_name, const Vector<Address> &p_arguments) = 0;
	virtual void write_super_call(const Address &p_target, const StringName &p_function_name, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_async(const Address &p_target, const Address &p_base, const StringName &p_function_name, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_utility(const Address &p_target, const StringName &p_function, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_gdscript_utility(const Address &p_target, const StringName &p_function, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_builtin_type(const Address &p_target, const Address &p_base, Variant::Type p_type, const StringName &p_method, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_builtin_type_static(const Address &p_target, Variant::Type p_type, const StringName &p_method, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_native_static(const Address &p_target, const StringName &p_class, const StringName &p_method, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_native_static_validated(const Address &p_target, MethodBind *p_method, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_method_bind(const Address &p_target, const Address &p_base, MethodBind *p_method, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_method_bind_validated(const Address &p_target, const Address &p_base, MethodBind *p_method, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_self(const Address &p_target, const StringName &p_function_name, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_self_async(const Address &p_target, const StringName &p_function_name, const Vector<Address> &p_arguments) = 0;
	virtual void write_call_script_function(const Address &p_target, const Address &p_base, const StringName &p_function_name, const Vector<Address> &p_arguments) = 0;
	virtual void write_lambda(const Address &p_target, GDScriptFunction *p_function, const Vector<Address> &p_captures, bool p_use_self) = 0;
	virtual void write_construct(const Address &p_target, Variant::Type p_type, const Vector<Address> &p_arguments) = 0;
	virtual void write_construct_array(const Address &p_target, const Vector<Address> &p_arguments) = 0;
	virtual void write_construct_typed_array(const Address &p_target, const GDScriptDataType &p_element_type, const Vector<Address> &p_arguments) = 0;
	virtual void write_construct_dictionary(const Address &p_target, const Vector<Address> &p_arguments) = 0;
	virtual void write_construct_typed_dictionary(const Address &p_target, const GDScriptDataType &p_key_type, const GDScriptDataType &p_value_type, const Vector<Address> &p_arguments) = 0;
	virtual void write_await(const Address &p_target, const Address &p_operand) = 0;
	virtual void write_if(const Address &p_condition) = 0;
	virtual void write_else() = 0;
	virtual void write_endif() = 0;
	virtual void write_jump_if_shared(const Address &p_value) = 0;
	virtual void write_end_jump_if_shared() = 0;
	virtual void start_for(const GDScriptDataType &p_iterator_type, const GDScriptDataType &p_list_type) = 0;
	virtual void write_for_assignment(const Address &p_list) = 0;
	virtual void write_for(const Address &p_variable, bool p_use_conversion) = 0;
	virtual void write_endfor() = 0;
	virtual void start_while_condition() = 0; // Used to allow a jump to the expression evaluation.
	virtual void write_while(const Address &p_condition) = 0;
	virtual void write_endwhile() = 0;
	virtual void write_break() = 0;
	virtual void write_continue() = 0;
	virtual void write_breakpoint() = 0;
	virtual void write_newline(int p_line) = 0;
	virtual void write_return(const Address &p_return_value) = 0;
	virtual void write_assert(const Address &p_test, const Address &p_message) = 0;

	virtual ~GDScriptCodeGenerator() {}
};
