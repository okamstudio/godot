/**************************************************************************/
/*  gdscript_byte_codegen.h                                               */
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

#include "gdscript_codegen.h"
#include "gdscript_function.h"
#include "gdscript_utility_functions.h"

class GDScriptByteCodeGenerator : public GDScriptCodeGenerator {
	struct StackSlot {
		Variant::Type type = Variant::NIL;
		bool can_contain_object = true;
		Vector<int> bytecode_indices;

		StackSlot() = default;
		StackSlot(Variant::Type p_type, bool p_can_contain_object) :
				type(p_type), can_contain_object(p_can_contain_object) {}
	};

	struct CallTarget {
		Address target;
		bool is_new_temporary = false;
		GDScriptByteCodeGenerator *codegen = nullptr;
#ifdef DEV_ENABLED
		bool cleaned = false;
#endif

		void cleanup() {
			DEV_ASSERT(!cleaned);
			if (is_new_temporary) {
				codegen->pop_temporary();
			}
#ifdef DEV_ENABLED
			cleaned = true;
#endif
		}

		CallTarget(Address p_target, bool p_is_new_temporary, GDScriptByteCodeGenerator *p_codegen) :
				target(p_target),
				is_new_temporary(p_is_new_temporary),
				codegen(p_codegen) {}
		~CallTarget() { DEV_ASSERT(cleaned); }
		CallTarget(const CallTarget &) = delete;
		CallTarget &operator=(CallTarget &) = delete;
	};

	bool ended = false;
	GDScriptFunction *function = nullptr;
	bool debug_stack = false;

	Vector<int> opcodes;
	List<RBMap<StringName, int>> stack_id_stack;
	RBMap<StringName, int> stack_identifiers;
	List<int> stack_identifiers_counts;
	RBMap<StringName, int> local_constants;

	Vector<StackSlot> locals;
	HashSet<int> dirty_locals;

	Vector<StackSlot> temporaries;
	List<int> used_temporaries;
	HashSet<int> temporaries_pending_clear;
	RBMap<Variant::Type, List<int>> temporaries_pool;

	List<GDScriptFunction::StackDebug> stack_debug;
	List<RBMap<StringName, int>> block_identifier_stack;
	RBMap<StringName, int> block_identifiers;

	int max_locals = 0;
	int current_line = 0;
	int instr_args_max = 0;

#ifdef DEBUG_ENABLED
	List<int> temp_stack;
#endif

	HashMap<Variant, int, VariantHasher, VariantComparator> constant_map;
	RBMap<StringName, int> name_map;
#ifdef TOOLS_ENABLED
	Vector<StringName> named_globals;
#endif
	RBMap<Variant::ValidatedOperatorEvaluator, int> operator_func_map;
	RBMap<Variant::ValidatedSetter, int> setters_map;
	RBMap<Variant::ValidatedGetter, int> getters_map;
	RBMap<Variant::ValidatedKeyedSetter, int> keyed_setters_map;
	RBMap<Variant::ValidatedKeyedGetter, int> keyed_getters_map;
	RBMap<Variant::ValidatedIndexedSetter, int> indexed_setters_map;
	RBMap<Variant::ValidatedIndexedGetter, int> indexed_getters_map;
	RBMap<Variant::ValidatedBuiltInMethod, int> builtin_method_map;
	RBMap<Variant::ValidatedConstructor, int> constructors_map;
	RBMap<Variant::ValidatedUtilityFunction, int> utilities_map;
	RBMap<GDScriptUtilityFunctions::FunctionPtr, int> gds_utilities_map;
	RBMap<MethodBind *, int> method_bind_map;
	RBMap<GDScriptFunction *, int> lambdas_map;

#ifdef DEBUG_ENABLED
	// Keep method and property names for pointer and validated operations.
	// Used when disassembling the bytecode.
	Vector<String> operator_names;
	Vector<String> setter_names;
	Vector<String> getter_names;
	Vector<String> builtin_methods_names;
	Vector<String> constructors_names;
	Vector<String> utilities_names;
	Vector<String> gds_utilities_names;
	void add_debug_name(Vector<String> &vector, int index, const String &name) {
		if (index >= vector.size()) {
			vector.resize(index + 1);
		}
		vector.write[index] = name;
	}
#endif

	// Lists since these can be nested.
	List<int> if_jmp_addrs;
	List<int> for_jmp_addrs;
	List<Address> for_counter_variables;
	List<Address> for_container_variables;
	List<int> while_jmp_addrs;
	List<int> continue_addrs;

	// Used to patch jumps with `and` and `or` operators with short-circuit.
	List<int> logic_op_jump_pos1;
	List<int> logic_op_jump_pos2;

	List<Address> ternary_result;
	List<int> ternary_jump_fail_pos;
	List<int> ternary_jump_skip_pos;

	List<List<int>> current_breaks_to_patch;

	void add_stack_identifier(const StringName &p_id, int p_stackpos) {
		if (locals.size() > max_locals) {
			max_locals = locals.size();
		}
		stack_identifiers[p_id] = p_stackpos;
		if (debug_stack) {
			block_identifiers[p_id] = p_stackpos;
			GDScriptFunction::StackDebug sd;
			sd.added = true;
			sd.line = current_line;
			sd.identifier = p_id;
			sd.pos = p_stackpos;
			stack_debug.push_back(sd);
		}
	}

	void push_stack_identifiers() {
		stack_identifiers_counts.push_back(locals.size());
		stack_id_stack.push_back(stack_identifiers);
		if (debug_stack) {
			RBMap<StringName, int> block_ids(block_identifiers);
			block_identifier_stack.push_back(block_ids);
			block_identifiers.clear();
		}
	}

	void pop_stack_identifiers() {
		int current_locals = stack_identifiers_counts.back()->get();
		stack_identifiers_counts.pop_back();
		stack_identifiers = stack_id_stack.back()->get();
		stack_id_stack.pop_back();
#ifdef DEBUG_ENABLED
		if (!used_temporaries.is_empty()) {
			ERR_PRINT("Leaving block with non-zero temporary variables: " + itos(used_temporaries.size()));
		}
#endif
		for (int i = current_locals; i < locals.size(); i++) {
			dirty_locals.insert(i + GDScriptFunction::FIXED_ADDRESSES_MAX);
		}
		locals.resize(current_locals);
		if (debug_stack) {
			for (const KeyValue<StringName, int> &E : block_identifiers) {
				GDScriptFunction::StackDebug sd;
				sd.added = false;
				sd.identifier = E.key;
				sd.line = current_line;
				sd.pos = E.value;
				stack_debug.push_back(sd);
			}
			block_identifiers = block_identifier_stack.back()->get();
			block_identifier_stack.pop_back();
		}
	}

	int get_name_map_pos(const StringName &p_identifier) {
		int ret;
		if (!name_map.has(p_identifier)) {
			ret = name_map.size();
			name_map[p_identifier] = ret;
		} else {
			ret = name_map[p_identifier];
		}
		return ret;
	}

	int get_constant_pos(const Variant &p_constant) {
		if (constant_map.has(p_constant)) {
			return constant_map[p_constant];
		}
		int pos = constant_map.size();
		constant_map[p_constant] = pos;
		return pos;
	}

	int get_operation_pos(const Variant::ValidatedOperatorEvaluator p_operation) {
		if (operator_func_map.has(p_operation)) {
			return operator_func_map[p_operation];
		}
		int pos = operator_func_map.size();
		operator_func_map[p_operation] = pos;
		return pos;
	}

	int get_setter_pos(const Variant::ValidatedSetter p_setter) {
		if (setters_map.has(p_setter)) {
			return setters_map[p_setter];
		}
		int pos = setters_map.size();
		setters_map[p_setter] = pos;
		return pos;
	}

	int get_getter_pos(const Variant::ValidatedGetter p_getter) {
		if (getters_map.has(p_getter)) {
			return getters_map[p_getter];
		}
		int pos = getters_map.size();
		getters_map[p_getter] = pos;
		return pos;
	}

	int get_keyed_setter_pos(const Variant::ValidatedKeyedSetter p_keyed_setter) {
		if (keyed_setters_map.has(p_keyed_setter)) {
			return keyed_setters_map[p_keyed_setter];
		}
		int pos = keyed_setters_map.size();
		keyed_setters_map[p_keyed_setter] = pos;
		return pos;
	}

	int get_keyed_getter_pos(const Variant::ValidatedKeyedGetter p_keyed_getter) {
		if (keyed_getters_map.has(p_keyed_getter)) {
			return keyed_getters_map[p_keyed_getter];
		}
		int pos = keyed_getters_map.size();
		keyed_getters_map[p_keyed_getter] = pos;
		return pos;
	}

	int get_indexed_setter_pos(const Variant::ValidatedIndexedSetter p_indexed_setter) {
		if (indexed_setters_map.has(p_indexed_setter)) {
			return indexed_setters_map[p_indexed_setter];
		}
		int pos = indexed_setters_map.size();
		indexed_setters_map[p_indexed_setter] = pos;
		return pos;
	}

	int get_indexed_getter_pos(const Variant::ValidatedIndexedGetter p_indexed_getter) {
		if (indexed_getters_map.has(p_indexed_getter)) {
			return indexed_getters_map[p_indexed_getter];
		}
		int pos = indexed_getters_map.size();
		indexed_getters_map[p_indexed_getter] = pos;
		return pos;
	}

	int get_builtin_method_pos(const Variant::ValidatedBuiltInMethod p_method) {
		if (builtin_method_map.has(p_method)) {
			return builtin_method_map[p_method];
		}
		int pos = builtin_method_map.size();
		builtin_method_map[p_method] = pos;
		return pos;
	}

	int get_constructor_pos(const Variant::ValidatedConstructor p_constructor) {
		if (constructors_map.has(p_constructor)) {
			return constructors_map[p_constructor];
		}
		int pos = constructors_map.size();
		constructors_map[p_constructor] = pos;
		return pos;
	}

	int get_utility_pos(const Variant::ValidatedUtilityFunction p_utility) {
		if (utilities_map.has(p_utility)) {
			return utilities_map[p_utility];
		}
		int pos = utilities_map.size();
		utilities_map[p_utility] = pos;
		return pos;
	}

	int get_gds_utility_pos(const GDScriptUtilityFunctions::FunctionPtr p_gds_utility) {
		if (gds_utilities_map.has(p_gds_utility)) {
			return gds_utilities_map[p_gds_utility];
		}
		int pos = gds_utilities_map.size();
		gds_utilities_map[p_gds_utility] = pos;
		return pos;
	}

	int get_method_bind_pos(MethodBind *p_method) {
		if (method_bind_map.has(p_method)) {
			return method_bind_map[p_method];
		}
		int pos = method_bind_map.size();
		method_bind_map[p_method] = pos;
		return pos;
	}

	int get_lambda_function_pos(GDScriptFunction *p_lambda_function) {
		if (lambdas_map.has(p_lambda_function)) {
			return lambdas_map[p_lambda_function];
		}
		int pos = lambdas_map.size();
		lambdas_map[p_lambda_function] = pos;
		return pos;
	}

	CallTarget get_call_target(const Address &p_target, Variant::Type p_type = Variant::NIL);

	int address_of(const Address &p_address) {
		switch (p_address.mode) {
			case Address::SELF:
				return GDScriptFunction::ADDR_SELF;
			case Address::CLASS:
				return GDScriptFunction::ADDR_CLASS;
			case Address::MEMBER:
				return p_address.address | (GDScriptFunction::ADDR_TYPE_MEMBER << GDScriptFunction::ADDR_BITS);
			case Address::CONSTANT:
				return p_address.address | (GDScriptFunction::ADDR_TYPE_CONSTANT << GDScriptFunction::ADDR_BITS);
			case Address::LOCAL_VARIABLE:
			case Address::FUNCTION_PARAMETER:
				return p_address.address | (GDScriptFunction::ADDR_TYPE_STACK << GDScriptFunction::ADDR_BITS);
			case Address::TEMPORARY:
				temporaries.write[p_address.address].bytecode_indices.push_back(opcodes.size());
				return -1;
			case Address::NIL:
				return GDScriptFunction::ADDR_NIL;
		}
		return -1; // Unreachable.
	}

	void append_opcode(GDScriptFunction::Opcode p_code) {
		opcodes.push_back(p_code);
	}

	void append_opcode_and_argcount(GDScriptFunction::Opcode p_code, int p_argument_count) {
		opcodes.push_back(p_code);
		opcodes.push_back(p_argument_count);
		instr_args_max = MAX(instr_args_max, p_argument_count);
	}

	void append(int p_code) {
		opcodes.push_back(p_code);
	}

	void append(const Address &p_address) {
		opcodes.push_back(address_of(p_address));
	}

	void append(const StringName &p_name) {
		opcodes.push_back(get_name_map_pos(p_name));
	}

	void append(const Variant::ValidatedOperatorEvaluator p_operation) {
		opcodes.push_back(get_operation_pos(p_operation));
	}

	void append(const Variant::ValidatedSetter p_setter) {
		opcodes.push_back(get_setter_pos(p_setter));
	}

	void append(const Variant::ValidatedGetter p_getter) {
		opcodes.push_back(get_getter_pos(p_getter));
	}

	void append(const Variant::ValidatedKeyedSetter p_keyed_setter) {
		opcodes.push_back(get_keyed_setter_pos(p_keyed_setter));
	}

	void append(const Variant::ValidatedKeyedGetter p_keyed_getter) {
		opcodes.push_back(get_keyed_getter_pos(p_keyed_getter));
	}

	void append(const Variant::ValidatedIndexedSetter p_indexed_setter) {
		opcodes.push_back(get_indexed_setter_pos(p_indexed_setter));
	}

	void append(const Variant::ValidatedIndexedGetter p_indexed_getter) {
		opcodes.push_back(get_indexed_getter_pos(p_indexed_getter));
	}

	void append(const Variant::ValidatedBuiltInMethod p_method) {
		opcodes.push_back(get_builtin_method_pos(p_method));
	}

	void append(const Variant::ValidatedConstructor p_constructor) {
		opcodes.push_back(get_constructor_pos(p_constructor));
	}

	void append(const Variant::ValidatedUtilityFunction p_utility) {
		opcodes.push_back(get_utility_pos(p_utility));
	}

	void append(const GDScriptUtilityFunctions::FunctionPtr p_gds_utility) {
		opcodes.push_back(get_gds_utility_pos(p_gds_utility));
	}

	void append(MethodBind *p_method) {
		opcodes.push_back(get_method_bind_pos(p_method));
	}

	void append(GDScriptFunction *p_lambda_function) {
		opcodes.push_back(get_lambda_function_pos(p_lambda_function));
	}

	void patch_jump(int p_address) {
		opcodes.write[p_address] = opcodes.size();
	}

public:
	virtual uint32_t add_parameter(const StringName &p_name, bool p_is_optional, const GDScriptDataType &p_type) override;
	virtual uint32_t add_local(const StringName &p_name, const GDScriptDataType &p_type) override;
	virtual uint32_t add_local_constant(const StringName &p_name, const Variant &p_constant) override;
	virtual uint32_t add_or_get_constant(const Variant &p_constant) override;
	virtual uint32_t add_or_get_name(const StringName &p_name) override;
	virtual uint32_t add_temporary(const GDScriptDataType &p_type) override;
	virtual void pop_temporary() override;
	virtual void clear_temporaries() override;
	virtual void clear_address(const Address &p_address) override;
	virtual bool is_local_dirty(const Address &p_address) const override;

	virtual void start_parameters() override;
	virtual void end_parameters() override;

	virtual void start_block() override;
	virtual void end_block() override;

	virtual void write_start(GDScript *p_script, const StringName &p_function_name, bool p_static, Variant p_rpc_config, const GDScriptDataType &p_return_type) override;
	virtual GDScriptFunction *write_end() override;

#ifdef DEBUG_ENABLED
	virtual void set_signature(const String &p_signature) override;
#endif
	virtual void set_initial_line(int p_line) override;

	virtual void write_type_adjust(const Address &p_target, Variant::Type p_new_type) override;
	virtual void write_unary_operator(const Address &p_target, Variant::Operator p_operator, const Address &p_left_operand) override;
	virtual void write_binary_operator(const Address &p_target, Variant::Operator p_operator, const Address &p_left_operand, const Address &p_right_operand) override;
	virtual void write_type_test(const Address &p_target, const Address &p_source, const GDScriptDataType &p_type) override;
	virtual void write_and_left_operand(const Address &p_left_operand) override;
	virtual void write_and_right_operand(const Address &p_right_operand) override;
	virtual void write_end_and(const Address &p_target) override;
	virtual void write_or_left_operand(const Address &p_left_operand) override;
	virtual void write_or_right_operand(const Address &p_right_operand) override;
	virtual void write_end_or(const Address &p_target) override;
	virtual void write_start_ternary(const Address &p_target) override;
	virtual void write_ternary_condition(const Address &p_condition) override;
	virtual void write_ternary_true_expr(const Address &p_expr) override;
	virtual void write_ternary_false_expr(const Address &p_expr) override;
	virtual void write_end_ternary() override;
	virtual void write_set(const Address &p_target, const Address &p_index, const Address &p_source) override;
	virtual void write_get(const Address &p_target, const Address &p_index, const Address &p_source) override;
	virtual void write_set_named(const Address &p_target, const StringName &p_name, const Address &p_source) override;
	virtual void write_get_named(const Address &p_target, const StringName &p_name, const Address &p_source) override;
	virtual void write_set_member(const Address &p_value, const StringName &p_name) override;
	virtual void write_get_member(const Address &p_target, const StringName &p_name) override;
	virtual void write_set_static_variable(const Address &p_value, const Address &p_class, int p_index) override;
	virtual void write_get_static_variable(const Address &p_target, const Address &p_class, int p_index) override;
	virtual void write_assign(const Address &p_target, const Address &p_source) override;
	virtual void write_assign_with_conversion(const Address &p_target, const Address &p_source) override;
	virtual void write_assign_null(const Address &p_target) override;
	virtual void write_assign_true(const Address &p_target) override;
	virtual void write_assign_false(const Address &p_target) override;
	virtual void write_assign_default_parameter(const Address &p_dst, const Address &p_src, bool p_use_conversion) override;
	virtual void write_store_global(const Address &p_dst, int p_global_index) override;
	virtual void write_store_named_global(const Address &p_dst, const StringName &p_global) override;
	virtual void write_cast(const Address &p_target, const Address &p_source, const GDScriptDataType &p_type) override;
	virtual void write_call(const Address &p_target, const Address &p_base, const StringName &p_function_name, const Vector<Address> &p_arguments) override;
	virtual void write_super_call(const Address &p_target, const StringName &p_function_name, const Vector<Address> &p_arguments) override;
	virtual void write_call_async(const Address &p_target, const Address &p_base, const StringName &p_function_name, const Vector<Address> &p_arguments) override;
	virtual void write_call_utility(const Address &p_target, const StringName &p_function, const Vector<Address> &p_arguments) override;
	void write_call_builtin_type(const Address &p_target, const Address &p_base, Variant::Type p_type, const StringName &p_method, bool p_is_static, const Vector<Address> &p_arguments);
	virtual void write_call_gdscript_utility(const Address &p_target, const StringName &p_function, const Vector<Address> &p_arguments) override;
	virtual void write_call_builtin_type(const Address &p_target, const Address &p_base, Variant::Type p_type, const StringName &p_method, const Vector<Address> &p_arguments) override;
	virtual void write_call_builtin_type_static(const Address &p_target, Variant::Type p_type, const StringName &p_method, const Vector<Address> &p_arguments) override;
	virtual void write_call_native_static(const Address &p_target, const StringName &p_class, const StringName &p_method, const Vector<Address> &p_arguments) override;
	virtual void write_call_native_static_validated(const Address &p_target, MethodBind *p_method, const Vector<Address> &p_arguments) override;
	virtual void write_call_method_bind(const Address &p_target, const Address &p_base, MethodBind *p_method, const Vector<Address> &p_arguments) override;
	virtual void write_call_method_bind_validated(const Address &p_target, const Address &p_base, MethodBind *p_method, const Vector<Address> &p_arguments) override;
	virtual void write_call_self(const Address &p_target, const StringName &p_function_name, const Vector<Address> &p_arguments) override;
	virtual void write_call_self_async(const Address &p_target, const StringName &p_function_name, const Vector<Address> &p_arguments) override;
	virtual void write_call_script_function(const Address &p_target, const Address &p_base, const StringName &p_function_name, const Vector<Address> &p_arguments) override;
	virtual void write_lambda(const Address &p_target, GDScriptFunction *p_function, const Vector<Address> &p_captures, bool p_use_self) override;
	virtual void write_construct(const Address &p_target, Variant::Type p_type, const Vector<Address> &p_arguments) override;
	virtual void write_construct_array(const Address &p_target, const Vector<Address> &p_arguments) override;
	virtual void write_construct_typed_array(const Address &p_target, const GDScriptDataType &p_element_type, const Vector<Address> &p_arguments) override;
	virtual void write_construct_dictionary(const Address &p_target, const Vector<Address> &p_arguments) override;
	virtual void write_construct_typed_dictionary(const Address &p_target, const GDScriptDataType &p_key_type, const GDScriptDataType &p_value_type, const Vector<Address> &p_arguments) override;
	virtual void write_await(const Address &p_target, const Address &p_operand) override;
	virtual void write_if(const Address &p_condition) override;
	virtual void write_else() override;
	virtual void write_endif() override;
	virtual void write_jump_if_shared(const Address &p_value) override;
	virtual void write_end_jump_if_shared() override;
	virtual void start_for(const GDScriptDataType &p_iterator_type, const GDScriptDataType &p_list_type) override;
	virtual void write_for_assignment(const Address &p_list) override;
	virtual void write_for(const Address &p_variable, bool p_use_conversion) override;
	virtual void write_endfor() override;
	virtual void start_while_condition() override;
	virtual void write_while(const Address &p_condition) override;
	virtual void write_endwhile() override;
	virtual void write_break() override;
	virtual void write_continue() override;
	virtual void write_breakpoint() override;
	virtual void write_newline(int p_line) override;
	virtual void write_return(const Address &p_return_value) override;
	virtual void write_assert(const Address &p_test, const Address &p_message) override;

	virtual ~GDScriptByteCodeGenerator();
};
