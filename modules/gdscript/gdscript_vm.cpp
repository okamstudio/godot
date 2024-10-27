/**************************************************************************/
/*  gdscript_vm.cpp                                                       */
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

#include "gdscript.h"
#include "gdscript_function.h"
#include "gdscript_lambda_callable.h"

#include "core/os/os.h"

#ifdef DEBUG_ENABLED

static bool _profile_count_as_native(const Object *p_base_obj, const StringName &p_methodname) {
	if (!p_base_obj) {
		return false;
	}
	StringName cname = p_base_obj->get_class_name();
	if ((p_methodname == "new" && cname == "GDScript") || p_methodname == "call") {
		return false;
	}
	return ClassDB::class_exists(cname) && ClassDB::has_method(cname, p_methodname, false);
}

static String _get_element_type(Variant::Type builtin_type, const StringName &native_type, const Ref<Script> &script_type) {
	if (script_type.is_valid() && script_type->is_valid()) {
		return GDScript::debug_get_script_name(script_type);
	} else if (native_type != StringName()) {
		return native_type.operator String();
	} else {
		return Variant::get_type_name(builtin_type);
	}
}

static String _get_var_type(const Variant *p_var) {
	String basestr;

	if (p_var->get_type() == Variant::OBJECT) {
		bool was_freed;
		Object *bobj = p_var->get_validated_object_with_check(was_freed);
		if (!bobj) {
			if (was_freed) {
				basestr = "previously freed";
			} else {
				basestr = "null instance";
			}
		} else {
			if (bobj->is_class_ptr(GDScriptNativeClass::get_class_ptr_static())) {
				basestr = Object::cast_to<GDScriptNativeClass>(bobj)->get_name();
			} else {
				basestr = bobj->get_class();
				if (bobj->get_script_instance()) {
					basestr += " (" + GDScript::debug_get_script_name(bobj->get_script_instance()->get_script()) + ")";
				}
			}
		}

	} else {
		if (p_var->get_type() == Variant::ARRAY) {
			basestr = "Array";
			const Array *p_array = VariantInternal::get_array(p_var);
			if (p_array->is_typed()) {
				basestr += "[" + _get_element_type((Variant::Type)p_array->get_typed_builtin(), p_array->get_typed_class_name(), p_array->get_typed_script()) + "]";
			}
		} else if (p_var->get_type() == Variant::DICTIONARY) {
			basestr = "Dictionary";
			const Dictionary *p_dictionary = VariantInternal::get_dictionary(p_var);
			if (p_dictionary->is_typed()) {
				basestr += "[" + _get_element_type((Variant::Type)p_dictionary->get_typed_key_builtin(), p_dictionary->get_typed_key_class_name(), p_dictionary->get_typed_key_script()) +
						", " + _get_element_type((Variant::Type)p_dictionary->get_typed_value_builtin(), p_dictionary->get_typed_value_class_name(), p_dictionary->get_typed_value_script()) + "]";
			}
		} else {
			basestr = Variant::get_type_name(p_var->get_type());
		}
	}

	return basestr;
}

void GDScriptFunction::_profile_native_call(uint64_t p_t_taken, const String &p_func_name, const String &p_instance_class_name) {
	HashMap<String, Profile::NativeProfile>::Iterator inner_prof = profile.native_calls.find(p_func_name);
	if (inner_prof) {
		inner_prof->value.call_count += 1;
	} else {
		String sig = vformat("%s::0::%s%s%s", get_script()->get_script_path(), p_instance_class_name, p_instance_class_name.is_empty() ? "" : ".", p_func_name);
		inner_prof = profile.native_calls.insert(p_func_name, Profile::NativeProfile{ 1, 0, sig });
	}
	inner_prof->value.total_time += p_t_taken;
}

#endif // DEBUG_ENABLED

Variant GDScriptFunction::_get_default_variant_for_data_type(const GDScriptDataType &p_data_type) {
	if (p_data_type.kind == GDScriptDataType::BUILTIN) {
		if (p_data_type.builtin_type == Variant::ARRAY) {
			Array array;
			// Typed array.
			if (p_data_type.has_container_element_type(0)) {
				const GDScriptDataType &element_type = p_data_type.get_container_element_type(0);
				array.set_typed(element_type.builtin_type, element_type.native_type, element_type.script_type);
			}

			return array;
		} else if (p_data_type.builtin_type == Variant::DICTIONARY) {
			Dictionary dict;
			// Typed dictionary.
			if (p_data_type.has_container_element_types()) {
				const GDScriptDataType &key_type = p_data_type.get_container_element_type_or_variant(0);
				const GDScriptDataType &value_type = p_data_type.get_container_element_type_or_variant(1);
				dict.set_typed(key_type.builtin_type, key_type.native_type, key_type.script_type, value_type.builtin_type, value_type.native_type, value_type.script_type);
			}

			return dict;
		} else {
			Callable::CallError ce;
			Variant variant;
			Variant::construct(p_data_type.builtin_type, variant, nullptr, 0, ce);

			ERR_FAIL_COND_V(ce.error != Callable::CallError::CALL_OK, Variant());

			return variant;
		}
	}

	return Variant();
}

String GDScriptFunction::_get_call_error(const String &p_where, const Variant **p_argptrs, const Variant &p_ret, const Callable::CallError &p_err) const {
	switch (p_err.error) {
		case Callable::CallError::CALL_OK:
			return String();
		case Callable::CallError::CALL_ERROR_INVALID_METHOD:
			if (p_ret.get_type() == Variant::STRING && !p_ret.operator String().is_empty()) {
				return "Invalid call " + p_where + ": " + p_ret.operator String();
			}
			return "Invalid call. Nonexistent " + p_where + ".";
		case Callable::CallError::CALL_ERROR_INVALID_ARGUMENT:
			ERR_FAIL_COND_V_MSG(p_err.argument < 0 || p_argptrs[p_err.argument] == nullptr, "Bug: Invalid CallError argument index or null pointer.", "Bug: Invalid CallError argument index or null pointer.");
			// Handle the Object to Object case separately as we don't have further class details.
#ifdef DEBUG_ENABLED
			if (p_err.expected == Variant::OBJECT && p_argptrs[p_err.argument]->get_type() == p_err.expected) {
				return "Invalid type in " + p_where + ". The Object-derived class of argument " + itos(p_err.argument + 1) + " (" + _get_var_type(p_argptrs[p_err.argument]) + ") is not a subclass of the expected argument class.";
			}
			if (p_err.expected == Variant::ARRAY && p_argptrs[p_err.argument]->get_type() == p_err.expected) {
				return "Invalid type in " + p_where + ". The array of argument " + itos(p_err.argument + 1) + " (" + _get_var_type(p_argptrs[p_err.argument]) + ") does not have the same element type as the expected typed array argument.";
			}
			if (p_err.expected == Variant::DICTIONARY && p_argptrs[p_err.argument]->get_type() == p_err.expected) {
				return "Invalid type in " + p_where + ". The dictionary of argument " + itos(p_err.argument + 1) + " (" + _get_var_type(p_argptrs[p_err.argument]) + ") does not have the same element type as the expected typed dictionary argument.";
			}
#endif // DEBUG_ENABLED
			return "Invalid type in " + p_where + ". Cannot convert argument " + itos(p_err.argument + 1) + " from " + Variant::get_type_name(p_argptrs[p_err.argument]->get_type()) + " to " + Variant::get_type_name(Variant::Type(p_err.expected)) + ".";
		case Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS:
		case Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS:
			return "Invalid call to " + p_where + ". Expected " + itos(p_err.expected) + " arguments.";
		case Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL:
			return "Attempt to call " + p_where + " on a null instance.";
		case Callable::CallError::CALL_ERROR_METHOD_NOT_CONST:
			return "Attempt to call " + p_where + " on a const instance.";
	}
	return "Bug: Invalid call error code " + itos(p_err.error) + ".";
}

void (*type_init_function_table[])(Variant *) = {
	nullptr, // NIL (shouldn't be called).
	&VariantInitializer<bool>::init, // BOOL.
	&VariantInitializer<int64_t>::init, // INT.
	&VariantInitializer<double>::init, // FLOAT.
	&VariantInitializer<String>::init, // STRING.
	&VariantInitializer<Vector2>::init, // VECTOR2.
	&VariantInitializer<Vector2i>::init, // VECTOR2I.
	&VariantInitializer<Rect2>::init, // RECT2.
	&VariantInitializer<Rect2i>::init, // RECT2I.
	&VariantInitializer<Vector3>::init, // VECTOR3.
	&VariantInitializer<Vector3i>::init, // VECTOR3I.
	&VariantInitializer<Transform2D>::init, // TRANSFORM2D.
	&VariantInitializer<Vector4>::init, // VECTOR4.
	&VariantInitializer<Vector4i>::init, // VECTOR4I.
	&VariantInitializer<Plane>::init, // PLANE.
	&VariantInitializer<Quaternion>::init, // QUATERNION.
	&VariantInitializer<AABB>::init, // AABB.
	&VariantInitializer<Basis>::init, // BASIS.
	&VariantInitializer<Transform3D>::init, // TRANSFORM3D.
	&VariantInitializer<Projection>::init, // PROJECTION.
	&VariantInitializer<Color>::init, // COLOR.
	&VariantInitializer<StringName>::init, // STRING_NAME.
	&VariantInitializer<NodePath>::init, // NODE_PATH.
	&VariantInitializer<RID>::init, // RID.
	&VariantInitializer<Object *>::init, // OBJECT.
	&VariantInitializer<Callable>::init, // CALLABLE.
	&VariantInitializer<Signal>::init, // SIGNAL.
	&VariantInitializer<Dictionary>::init, // DICTIONARY.
	&VariantInitializer<Array>::init, // ARRAY.
	&VariantInitializer<PackedByteArray>::init, // PACKED_BYTE_ARRAY.
	&VariantInitializer<PackedInt32Array>::init, // PACKED_INT32_ARRAY.
	&VariantInitializer<PackedInt64Array>::init, // PACKED_INT64_ARRAY.
	&VariantInitializer<PackedFloat32Array>::init, // PACKED_FLOAT32_ARRAY.
	&VariantInitializer<PackedFloat64Array>::init, // PACKED_FLOAT64_ARRAY.
	&VariantInitializer<PackedStringArray>::init, // PACKED_STRING_ARRAY.
	&VariantInitializer<PackedVector2Array>::init, // PACKED_VECTOR2_ARRAY.
	&VariantInitializer<PackedVector3Array>::init, // PACKED_VECTOR3_ARRAY.
	&VariantInitializer<PackedColorArray>::init, // PACKED_COLOR_ARRAY.
	&VariantInitializer<PackedVector4Array>::init, // PACKED_VECTOR4_ARRAY.
};

#if defined(__GNUC__) || defined(__clang__)
#define OPCODES_TABLE                                    \
	static const void *switch_table_ops[] = {            \
		&&OPCODE_OPERATOR,                               \
		&&OPCODE_OPERATOR_VALIDATED,                     \
		&&OPCODE_TYPE_TEST_BUILTIN,                      \
		&&OPCODE_TYPE_TEST_ARRAY,                        \
		&&OPCODE_TYPE_TEST_DICTIONARY,                   \
		&&OPCODE_TYPE_TEST_NATIVE,                       \
		&&OPCODE_TYPE_TEST_SCRIPT,                       \
		&&OPCODE_SET_KEYED,                              \
		&&OPCODE_SET_KEYED_VALIDATED,                    \
		&&OPCODE_SET_INDEXED_VALIDATED,                  \
		&&OPCODE_GET_KEYED,                              \
		&&OPCODE_GET_KEYED_VALIDATED,                    \
		&&OPCODE_GET_INDEXED_VALIDATED,                  \
		&&OPCODE_SET_NAMED,                              \
		&&OPCODE_SET_NAMED_VALIDATED,                    \
		&&OPCODE_GET_NAMED,                              \
		&&OPCODE_GET_NAMED_VALIDATED,                    \
		&&OPCODE_SET_MEMBER,                             \
		&&OPCODE_GET_MEMBER,                             \
		&&OPCODE_SET_STATIC_VARIABLE,                    \
		&&OPCODE_GET_STATIC_VARIABLE,                    \
		&&OPCODE_ASSIGN,                                 \
		&&OPCODE_ASSIGN_NULL,                            \
		&&OPCODE_ASSIGN_TRUE,                            \
		&&OPCODE_ASSIGN_FALSE,                           \
		&&OPCODE_ASSIGN_TYPED_BUILTIN,                   \
		&&OPCODE_ASSIGN_TYPED_ARRAY,                     \
		&&OPCODE_ASSIGN_TYPED_DICTIONARY,                \
		&&OPCODE_ASSIGN_TYPED_NATIVE,                    \
		&&OPCODE_ASSIGN_TYPED_SCRIPT,                    \
		&&OPCODE_CAST_TO_BUILTIN,                        \
		&&OPCODE_CAST_TO_NATIVE,                         \
		&&OPCODE_CAST_TO_SCRIPT,                         \
		&&OPCODE_CONSTRUCT,                              \
		&&OPCODE_CONSTRUCT_VALIDATED,                    \
		&&OPCODE_CONSTRUCT_ARRAY,                        \
		&&OPCODE_CONSTRUCT_TYPED_ARRAY,                  \
		&&OPCODE_CONSTRUCT_DICTIONARY,                   \
		&&OPCODE_CONSTRUCT_TYPED_DICTIONARY,             \
		&&OPCODE_CALL,                                   \
		&&OPCODE_CALL_RETURN,                            \
		&&OPCODE_CALL_ASYNC,                             \
		&&OPCODE_CALL_UTILITY,                           \
		&&OPCODE_CALL_UTILITY_VALIDATED,                 \
		&&OPCODE_CALL_GDSCRIPT_UTILITY,                  \
		&&OPCODE_CALL_BUILTIN_TYPE_VALIDATED,            \
		&&OPCODE_CALL_SELF_BASE,                         \
		&&OPCODE_CALL_METHOD_BIND,                       \
		&&OPCODE_CALL_METHOD_BIND_RET,                   \
		&&OPCODE_CALL_BUILTIN_STATIC,                    \
		&&OPCODE_CALL_NATIVE_STATIC,                     \
		&&OPCODE_CALL_NATIVE_STATIC_VALIDATED_RETURN,    \
		&&OPCODE_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN, \
		&&OPCODE_CALL_METHOD_BIND_VALIDATED_RETURN,      \
		&&OPCODE_CALL_METHOD_BIND_VALIDATED_NO_RETURN,   \
		&&OPCODE_AWAIT,                                  \
		&&OPCODE_AWAIT_RESUME,                           \
		&&OPCODE_CREATE_LAMBDA,                          \
		&&OPCODE_CREATE_SELF_LAMBDA,                     \
		&&OPCODE_JUMP,                                   \
		&&OPCODE_JUMP_IF,                                \
		&&OPCODE_JUMP_IF_NOT,                            \
		&&OPCODE_JUMP_TO_DEF_ARGUMENT,                   \
		&&OPCODE_JUMP_IF_SHARED,                         \
		&&OPCODE_RETURN,                                 \
		&&OPCODE_RETURN_TYPED_BUILTIN,                   \
		&&OPCODE_RETURN_TYPED_ARRAY,                     \
		&&OPCODE_RETURN_TYPED_DICTIONARY,                \
		&&OPCODE_RETURN_TYPED_NATIVE,                    \
		&&OPCODE_RETURN_TYPED_SCRIPT,                    \
		&&OPCODE_ITERATE_BEGIN,                          \
		&&OPCODE_ITERATE_BEGIN_INT,                      \
		&&OPCODE_ITERATE_BEGIN_FLOAT,                    \
		&&OPCODE_ITERATE_BEGIN_VECTOR2,                  \
		&&OPCODE_ITERATE_BEGIN_VECTOR2I,                 \
		&&OPCODE_ITERATE_BEGIN_VECTOR3,                  \
		&&OPCODE_ITERATE_BEGIN_VECTOR3I,                 \
		&&OPCODE_ITERATE_BEGIN_STRING,                   \
		&&OPCODE_ITERATE_BEGIN_DICTIONARY,               \
		&&OPCODE_ITERATE_BEGIN_ARRAY,                    \
		&&OPCODE_ITERATE_BEGIN_PACKED_BYTE_ARRAY,        \
		&&OPCODE_ITERATE_BEGIN_PACKED_INT32_ARRAY,       \
		&&OPCODE_ITERATE_BEGIN_PACKED_INT64_ARRAY,       \
		&&OPCODE_ITERATE_BEGIN_PACKED_FLOAT32_ARRAY,     \
		&&OPCODE_ITERATE_BEGIN_PACKED_FLOAT64_ARRAY,     \
		&&OPCODE_ITERATE_BEGIN_PACKED_STRING_ARRAY,      \
		&&OPCODE_ITERATE_BEGIN_PACKED_VECTOR2_ARRAY,     \
		&&OPCODE_ITERATE_BEGIN_PACKED_VECTOR3_ARRAY,     \
		&&OPCODE_ITERATE_BEGIN_PACKED_COLOR_ARRAY,       \
		&&OPCODE_ITERATE_BEGIN_PACKED_VECTOR4_ARRAY,     \
		&&OPCODE_ITERATE_BEGIN_OBJECT,                   \
		&&OPCODE_ITERATE,                                \
		&&OPCODE_ITERATE_INT,                            \
		&&OPCODE_ITERATE_FLOAT,                          \
		&&OPCODE_ITERATE_VECTOR2,                        \
		&&OPCODE_ITERATE_VECTOR2I,                       \
		&&OPCODE_ITERATE_VECTOR3,                        \
		&&OPCODE_ITERATE_VECTOR3I,                       \
		&&OPCODE_ITERATE_STRING,                         \
		&&OPCODE_ITERATE_DICTIONARY,                     \
		&&OPCODE_ITERATE_ARRAY,                          \
		&&OPCODE_ITERATE_PACKED_BYTE_ARRAY,              \
		&&OPCODE_ITERATE_PACKED_INT32_ARRAY,             \
		&&OPCODE_ITERATE_PACKED_INT64_ARRAY,             \
		&&OPCODE_ITERATE_PACKED_FLOAT32_ARRAY,           \
		&&OPCODE_ITERATE_PACKED_FLOAT64_ARRAY,           \
		&&OPCODE_ITERATE_PACKED_STRING_ARRAY,            \
		&&OPCODE_ITERATE_PACKED_VECTOR2_ARRAY,           \
		&&OPCODE_ITERATE_PACKED_VECTOR3_ARRAY,           \
		&&OPCODE_ITERATE_PACKED_COLOR_ARRAY,             \
		&&OPCODE_ITERATE_PACKED_VECTOR4_ARRAY,           \
		&&OPCODE_ITERATE_OBJECT,                         \
		&&OPCODE_STORE_GLOBAL,                           \
		&&OPCODE_STORE_NAMED_GLOBAL,                     \
		&&OPCODE_TYPE_ADJUST_BOOL,                       \
		&&OPCODE_TYPE_ADJUST_INT,                        \
		&&OPCODE_TYPE_ADJUST_FLOAT,                      \
		&&OPCODE_TYPE_ADJUST_STRING,                     \
		&&OPCODE_TYPE_ADJUST_VECTOR2,                    \
		&&OPCODE_TYPE_ADJUST_VECTOR2I,                   \
		&&OPCODE_TYPE_ADJUST_RECT2,                      \
		&&OPCODE_TYPE_ADJUST_RECT2I,                     \
		&&OPCODE_TYPE_ADJUST_VECTOR3,                    \
		&&OPCODE_TYPE_ADJUST_VECTOR3I,                   \
		&&OPCODE_TYPE_ADJUST_TRANSFORM2D,                \
		&&OPCODE_TYPE_ADJUST_VECTOR4,                    \
		&&OPCODE_TYPE_ADJUST_VECTOR4I,                   \
		&&OPCODE_TYPE_ADJUST_PLANE,                      \
		&&OPCODE_TYPE_ADJUST_QUATERNION,                 \
		&&OPCODE_TYPE_ADJUST_AABB,                       \
		&&OPCODE_TYPE_ADJUST_BASIS,                      \
		&&OPCODE_TYPE_ADJUST_TRANSFORM3D,                \
		&&OPCODE_TYPE_ADJUST_PROJECTION,                 \
		&&OPCODE_TYPE_ADJUST_COLOR,                      \
		&&OPCODE_TYPE_ADJUST_STRING_NAME,                \
		&&OPCODE_TYPE_ADJUST_NODE_PATH,                  \
		&&OPCODE_TYPE_ADJUST_RID,                        \
		&&OPCODE_TYPE_ADJUST_OBJECT,                     \
		&&OPCODE_TYPE_ADJUST_CALLABLE,                   \
		&&OPCODE_TYPE_ADJUST_SIGNAL,                     \
		&&OPCODE_TYPE_ADJUST_DICTIONARY,                 \
		&&OPCODE_TYPE_ADJUST_ARRAY,                      \
		&&OPCODE_TYPE_ADJUST_PACKED_BYTE_ARRAY,          \
		&&OPCODE_TYPE_ADJUST_PACKED_INT32_ARRAY,         \
		&&OPCODE_TYPE_ADJUST_PACKED_INT64_ARRAY,         \
		&&OPCODE_TYPE_ADJUST_PACKED_FLOAT32_ARRAY,       \
		&&OPCODE_TYPE_ADJUST_PACKED_FLOAT64_ARRAY,       \
		&&OPCODE_TYPE_ADJUST_PACKED_STRING_ARRAY,        \
		&&OPCODE_TYPE_ADJUST_PACKED_VECTOR2_ARRAY,       \
		&&OPCODE_TYPE_ADJUST_PACKED_VECTOR3_ARRAY,       \
		&&OPCODE_TYPE_ADJUST_PACKED_COLOR_ARRAY,         \
		&&OPCODE_TYPE_ADJUST_PACKED_VECTOR4_ARRAY,       \
		&&OPCODE_ASSERT,                                 \
		&&OPCODE_BREAKPOINT,                             \
		&&OPCODE_LINE,                                   \
		&&OPCODE_END                                     \
	};                                                   \
	static_assert((sizeof(switch_table_ops) / sizeof(switch_table_ops[0]) == (OPCODE_END + 1)), "Opcodes in jump table aren't the same as opcodes in enum.");

#define OPCODE(m_op) \
	m_op:
#define OPCODE_WHILE(m_test)
#define OPCODES_END \
	OPSEXIT:
#define OPCODES_OUT \
	OPSOUT:
#define OPCODE_SWITCH(m_test) goto *switch_table_ops[m_test];

#ifdef DEBUG_ENABLED
#define DISPATCH_OPCODE          \
	last_opcode = _code_ptr[ip]; \
	goto *switch_table_ops[last_opcode]
#else // !DEBUG_ENABLED
#define DISPATCH_OPCODE goto *switch_table_ops[_code_ptr[ip]]
#endif // DEBUG_ENABLED

#define OPCODE_BREAK goto OPSEXIT
#define OPCODE_OUT goto OPSOUT
#else // !(defined(__GNUC__) || defined(__clang__))
#define OPCODES_TABLE
#define OPCODE(m_op) case m_op:
#define OPCODE_WHILE(m_test) while (m_test)
#define OPCODES_END
#define OPCODES_OUT
#define DISPATCH_OPCODE continue

#ifdef _MSC_VER
#define OPCODE_SWITCH(m_test)       \
	__assume(m_test <= OPCODE_END); \
	switch (m_test)
#else // !_MSC_VER
#define OPCODE_SWITCH(m_test) switch (m_test)
#endif // _MSC_VER

#define OPCODE_BREAK break
#define OPCODE_OUT break
#endif // defined(__GNUC__) || defined(__clang__)

// Helpers for VariantInternal methods in macros.
#define OP_GET_BOOL get_bool
#define OP_GET_INT get_int
#define OP_GET_FLOAT get_float
#define OP_GET_VECTOR2 get_vector2
#define OP_GET_VECTOR2I get_vector2i
#define OP_GET_VECTOR3 get_vector3
#define OP_GET_VECTOR3I get_vector3i
#define OP_GET_RECT2 get_rect2
#define OP_GET_VECTOR4 get_vector4
#define OP_GET_VECTOR4I get_vector4i
#define OP_GET_RECT2I get_rect2i
#define OP_GET_QUATERNION get_quaternion
#define OP_GET_COLOR get_color
#define OP_GET_STRING get_string
#define OP_GET_STRING_NAME get_string_name
#define OP_GET_NODE_PATH get_node_path
#define OP_GET_CALLABLE get_callable
#define OP_GET_SIGNAL get_signal
#define OP_GET_ARRAY get_array
#define OP_GET_DICTIONARY get_dictionary
#define OP_GET_PACKED_BYTE_ARRAY get_byte_array
#define OP_GET_PACKED_INT32_ARRAY get_int32_array
#define OP_GET_PACKED_INT64_ARRAY get_int64_array
#define OP_GET_PACKED_FLOAT32_ARRAY get_float32_array
#define OP_GET_PACKED_FLOAT64_ARRAY get_float64_array
#define OP_GET_PACKED_STRING_ARRAY get_string_array
#define OP_GET_PACKED_VECTOR2_ARRAY get_vector2_array
#define OP_GET_PACKED_VECTOR3_ARRAY get_vector3_array
#define OP_GET_PACKED_COLOR_ARRAY get_color_array
#define OP_GET_PACKED_VECTOR4_ARRAY get_vector4_array
#define OP_GET_TRANSFORM3D get_transform
#define OP_GET_TRANSFORM2D get_transform2d
#define OP_GET_PROJECTION get_projection
#define OP_GET_PLANE get_plane
#define OP_GET_AABB get_aabb
#define OP_GET_BASIS get_basis
#define OP_GET_RID get_rid

#define METHOD_CALL_ON_NULL_VALUE_ERROR(method_pointer) "Cannot call method '" + (method_pointer)->get_name() + "' on a null value."
#define METHOD_CALL_ON_FREED_INSTANCE_ERROR(method_pointer) "Cannot call method '" + (method_pointer)->get_name() + "' on a previously freed instance."

Variant GDScriptFunction::call(GDScriptInstance *p_instance, const Variant **p_args, int p_argcount, Callable::CallError &r_err, CallState *p_state) {
	OPCODES_TABLE;

	if (!_code_ptr) {
		return _get_default_variant_for_data_type(return_type);
	}

	r_err.error = Callable::CallError::CALL_OK;

	static thread_local int call_depth = 0;
	if (unlikely(++call_depth > MAX_CALL_DEPTH)) {
		call_depth--;
#ifdef DEBUG_ENABLED
		String err_file;
		if (p_instance && ObjectDB::get_instance(p_instance->owner_id) != nullptr && p_instance->script->is_valid() && !p_instance->script->path.is_empty()) {
			err_file = p_instance->script->path;
		} else if (_script) {
			err_file = _script->path;
		}
		if (err_file.is_empty()) {
			err_file = "<built-in>";
		}
		String err_func = name;
		if (p_instance && ObjectDB::get_instance(p_instance->owner_id) != nullptr && p_instance->script->is_valid() && p_instance->script->local_name != StringName()) {
			err_func = p_instance->script->local_name.operator String() + "." + err_func;
		}
		int err_line = _initial_line;
		const char *err_text = "Stack overflow. Check for infinite recursion in your script.";
		if (!GDScriptLanguage::get_singleton()->debug_break(err_text, false)) {
			// Debugger break did not happen.
			_err_print_error(err_func.utf8().get_data(), err_file.utf8().get_data(), err_line, err_text, false, ERR_HANDLER_SCRIPT);
		}
#endif
		return _get_default_variant_for_data_type(return_type);
	}

	Variant retvalue;
	Variant *stack = nullptr;
	Variant **instruction_args = nullptr;
	int defarg = 0;

#ifdef DEBUG_ENABLED

	//GDScriptLanguage::get_singleton()->calls++;

#endif

	uint32_t alloca_size = 0;
	GDScript *script;
	int ip = 0;
	int line = _initial_line;

	if (p_state) {
		//use existing (supplied) state (awaited)
		stack = (Variant *)p_state->stack.ptr();
		instruction_args = (Variant **)&p_state->stack.ptr()[sizeof(Variant) * p_state->stack_size]; //ptr() to avoid bounds check
		line = p_state->line;
		ip = p_state->ip;
		alloca_size = p_state->stack.size();
		script = p_state->script;
		p_instance = p_state->instance;
		defarg = p_state->defarg;

	} else {
		if (p_argcount != _argument_count) {
			if (p_argcount > _argument_count) {
				r_err.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
				r_err.expected = _argument_count;
				call_depth--;
				return _get_default_variant_for_data_type(return_type);
			} else if (p_argcount < _argument_count - _default_arg_count) {
				r_err.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
				r_err.expected = _argument_count - _default_arg_count;
				call_depth--;
				return _get_default_variant_for_data_type(return_type);
			} else {
				defarg = _argument_count - p_argcount;
			}
		}

		// Add 3 here for self, class, and nil.
		alloca_size = sizeof(Variant *) * 3 + sizeof(Variant *) * _instruction_args_size + sizeof(Variant) * _stack_size;

		uint8_t *aptr = (uint8_t *)alloca(alloca_size);
		stack = (Variant *)aptr;

		for (int i = 0; i < p_argcount; i++) {
			if (!argument_types[i].has_type) {
				memnew_placement(&stack[i + 3], Variant(*p_args[i]));
				continue;
			}
			// If types already match, don't call Variant::construct(). Constructors of some types
			// (e.g. packed arrays) do copies, whereas they pass by reference when inside a Variant.
			if (argument_types[i].is_type(*p_args[i], false)) {
				memnew_placement(&stack[i + 3], Variant(*p_args[i]));
				continue;
			}
			if (!argument_types[i].is_type(*p_args[i], true)) {
				r_err.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
				r_err.argument = i;
				r_err.expected = argument_types[i].builtin_type;
				call_depth--;
				return _get_default_variant_for_data_type(return_type);
			}
			if (argument_types[i].kind == GDScriptDataType::BUILTIN) {
				if (argument_types[i].builtin_type == Variant::DICTIONARY && argument_types[i].has_container_element_types()) {
					const GDScriptDataType &arg_key_type = argument_types[i].get_container_element_type_or_variant(0);
					const GDScriptDataType &arg_value_type = argument_types[i].get_container_element_type_or_variant(1);
					Dictionary dict(p_args[i]->operator Dictionary(), arg_key_type.builtin_type, arg_key_type.native_type, arg_key_type.script_type, arg_value_type.builtin_type, arg_value_type.native_type, arg_value_type.script_type);
					memnew_placement(&stack[i + 3], Variant(dict));
				} else if (argument_types[i].builtin_type == Variant::ARRAY && argument_types[i].has_container_element_type(0)) {
					const GDScriptDataType &arg_type = argument_types[i].container_element_types[0];
					Array array(p_args[i]->operator Array(), arg_type.builtin_type, arg_type.native_type, arg_type.script_type);
					memnew_placement(&stack[i + 3], Variant(array));
				} else {
					Variant variant;
					Variant::construct(argument_types[i].builtin_type, variant, &p_args[i], 1, r_err);
					if (unlikely(r_err.error)) {
						r_err.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
						r_err.argument = i;
						r_err.expected = argument_types[i].builtin_type;
						call_depth--;
						return _get_default_variant_for_data_type(return_type);
					}
					memnew_placement(&stack[i + 3], Variant(variant));
				}
			} else {
				memnew_placement(&stack[i + 3], Variant(*p_args[i]));
			}
		}
		for (int i = p_argcount + 3; i < _stack_size; i++) {
			memnew_placement(&stack[i], Variant);
		}

		if (_instruction_args_size) {
			instruction_args = (Variant **)&aptr[sizeof(Variant) * _stack_size];
		} else {
			instruction_args = nullptr;
		}

		for (const KeyValue<int, Variant::Type> &E : temporary_slots) {
			type_init_function_table[E.value](&stack[E.key]);
		}
	}

	if (p_instance) {
		memnew_placement(&stack[ADDR_STACK_SELF], Variant(p_instance->owner));
		script = p_instance->script.ptr();
	} else {
		memnew_placement(&stack[ADDR_STACK_SELF], Variant);
		script = _script;
	}
	memnew_placement(&stack[ADDR_STACK_CLASS], Variant(script));
	memnew_placement(&stack[ADDR_STACK_NIL], Variant);

	String err_text;

#ifdef DEBUG_ENABLED

	if (EngineDebugger::is_active()) {
		GDScriptLanguage::get_singleton()->enter_function(p_instance, this, stack, &ip, &line);
	}

#define GD_ERR_BREAK(m_cond)                                                                                           \
	{                                                                                                                  \
		if (unlikely(m_cond)) {                                                                                        \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' " _STR(m_cond) " ' is true. Breaking..:"); \
			OPCODE_BREAK;                                                                                              \
		}                                                                                                              \
	}

#define CHECK_SPACE(m_space) \
	GD_ERR_BREAK((ip + m_space) > _code_size)

#define GET_VARIANT_PTR(m_v, m_code_ofs)                                                            \
	Variant *m_v;                                                                                   \
	{                                                                                               \
		int address = _code_ptr[ip + 1 + (m_code_ofs)];                                             \
		int address_type = (address & ADDR_TYPE_MASK) >> ADDR_BITS;                                 \
		if (unlikely(address_type < 0 || address_type >= ADDR_TYPE_MAX)) {                          \
			err_text = "Bad address type.";                                                         \
			OPCODE_BREAK;                                                                           \
		}                                                                                           \
		int address_index = address & ADDR_MASK;                                                    \
		if (unlikely(address_index < 0 || address_index >= variant_address_limits[address_type])) { \
			if (address_type == ADDR_TYPE_MEMBER && !p_instance) {                                  \
				err_text = "Cannot access member without instance.";                                \
			} else {                                                                                \
				err_text = "Bad address index.";                                                    \
			}                                                                                       \
			OPCODE_BREAK;                                                                           \
		}                                                                                           \
		m_v = &variant_addresses[address_type][address_index];                                      \
		if (unlikely(!m_v))                                                                         \
			OPCODE_BREAK;                                                                           \
	}

#else // !DEBUG_ENABLED
#define GD_ERR_BREAK(m_cond)
#define CHECK_SPACE(m_space)

#define GET_VARIANT_PTR(m_v, m_code_ofs)                                                        \
	Variant *m_v;                                                                               \
	{                                                                                           \
		int address = _code_ptr[ip + 1 + (m_code_ofs)];                                         \
		m_v = &variant_addresses[(address & ADDR_TYPE_MASK) >> ADDR_BITS][address & ADDR_MASK]; \
		if (unlikely(!m_v))                                                                     \
			OPCODE_BREAK;                                                                       \
	}

#endif // DEBUG_ENABLED

#define LOAD_INSTRUCTION_ARGS                   \
	int instr_arg_count = _code_ptr[ip + 1];    \
	for (int i = 0; i < instr_arg_count; i++) { \
		GET_VARIANT_PTR(v, i + 1);              \
		instruction_args[i] = v;                \
	}                                           \
	ip += 1; // Offset to skip instruction argcount.

#define GET_INSTRUCTION_ARG(m_v, m_idx) \
	Variant *m_v = instruction_args[m_idx]

#ifdef DEBUG_ENABLED
	uint64_t function_start_time = 0;
	uint64_t function_call_time = 0;

	if (GDScriptLanguage::get_singleton()->profiling) {
		function_start_time = OS::get_singleton()->get_ticks_usec();
		function_call_time = 0;
		profile.call_count.increment();
		profile.frame_call_count.increment();
	}
	bool exit_ok = false;
	bool awaited = false;
	int variant_address_limits[ADDR_TYPE_MAX] = { _stack_size, _constant_count, p_instance ? (int)p_instance->members.size() : 0 };
#endif

	Variant *variant_addresses[ADDR_TYPE_MAX] = { stack, _constants_ptr, p_instance ? p_instance->members.ptrw() : nullptr };

#ifdef DEBUG_ENABLED
	OPCODE_WHILE(ip < _code_size) {
		int last_opcode = _code_ptr[ip];
#else
	OPCODE_WHILE(true) {
#endif

		OPCODE_SWITCH(_code_ptr[ip]) {
			OPCODE(OPCODE_OPERATOR) {
				constexpr int _pointer_size = sizeof(Variant::ValidatedOperatorEvaluator) / sizeof(*_code_ptr);
				CHECK_SPACE(7 + _pointer_size);

				bool valid;
				Variant::Operator op = (Variant::Operator)_code_ptr[ip + 4];
				GD_ERR_BREAK(op >= Variant::OP_MAX);

				GET_VARIANT_PTR(a, 0);
				GET_VARIANT_PTR(b, 1);
				GET_VARIANT_PTR(dst, 2);
				// Compute signatures (types of operands) so it can be optimized when matching.
				uint32_t op_signature = _code_ptr[ip + 5];
				uint32_t actual_signature = (a->get_type() << 8) | (b->get_type());

#ifdef DEBUG_ENABLED
				if (op == Variant::OP_DIVIDE || op == Variant::OP_MODULE) {
					// Don't optimize division and modulo since there's not check for division by zero with validated calls.
					op_signature = 0xFFFF;
					_code_ptr[ip + 5] = op_signature;
				}
#endif

				// Check if this is the first run. If so, store the current signature for the optimized path.
				if (unlikely(op_signature == 0)) {
					static Mutex initializer_mutex;
					initializer_mutex.lock();
					Variant::Type a_type = (Variant::Type)((actual_signature >> 8) & 0xFF);
					Variant::Type b_type = (Variant::Type)(actual_signature & 0xFF);

					Variant::ValidatedOperatorEvaluator op_func = Variant::get_validated_operator_evaluator(op, a_type, b_type);

					if (unlikely(!op_func)) {
#ifdef DEBUG_ENABLED
						err_text = "Invalid operands '" + Variant::get_type_name(a->get_type()) + "' and '" + Variant::get_type_name(b->get_type()) + "' in operator '" + Variant::get_operator_name(op) + "'.";
#endif
						initializer_mutex.unlock();
						OPCODE_BREAK;
					} else {
						Variant::Type ret_type = Variant::get_operator_return_type(op, a_type, b_type);
						VariantInternal::initialize(dst, ret_type);
						op_func(a, b, dst);

						// Check again in case another thread already set it.
						if (_code_ptr[ip + 5] == 0) {
							_code_ptr[ip + 5] = actual_signature;
							_code_ptr[ip + 6] = static_cast<int>(ret_type);
							Variant::ValidatedOperatorEvaluator *tmp = reinterpret_cast<Variant::ValidatedOperatorEvaluator *>(&_code_ptr[ip + 7]);
							*tmp = op_func;
						}
					}
					initializer_mutex.unlock();
				} else if (likely(op_signature == actual_signature)) {
					// If the signature matches, we can use the optimized path.
					Variant::Type ret_type = static_cast<Variant::Type>(_code_ptr[ip + 6]);
					Variant::ValidatedOperatorEvaluator op_func = *reinterpret_cast<Variant::ValidatedOperatorEvaluator *>(&_code_ptr[ip + 7]);

					// Make sure the return value has the correct type.
					VariantInternal::initialize(dst, ret_type);
					op_func(a, b, dst);
				} else {
					// If the signature doesn't match, we have to use the slow path.
#ifdef DEBUG_ENABLED

					Variant ret;
					Variant::evaluate(op, *a, *b, ret, valid);
#else
					Variant::evaluate(op, *a, *b, *dst, valid);
#endif
#ifdef DEBUG_ENABLED
					if (!valid) {
						if (ret.get_type() == Variant::STRING) {
							//return a string when invalid with the error
							err_text = ret;
							err_text += " in operator '" + Variant::get_operator_name(op) + "'.";
						} else {
							err_text = "Invalid operands '" + Variant::get_type_name(a->get_type()) + "' and '" + Variant::get_type_name(b->get_type()) + "' in operator '" + Variant::get_operator_name(op) + "'.";
						}
						OPCODE_BREAK;
					}
					*dst = ret;
#endif
				}
				ip += 7 + _pointer_size;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_OPERATOR_VALIDATED) {
				CHECK_SPACE(5);

				int operator_idx = _code_ptr[ip + 4];
				GD_ERR_BREAK(operator_idx < 0 || operator_idx >= _operator_funcs_count);
				Variant::ValidatedOperatorEvaluator operator_func = _operator_funcs_ptr[operator_idx];

				GET_VARIANT_PTR(a, 0);
				GET_VARIANT_PTR(b, 1);
				GET_VARIANT_PTR(dst, 2);

				operator_func(a, b, dst);

				ip += 5;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_TYPE_TEST_BUILTIN) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(value, 1);

				Variant::Type builtin_type = (Variant::Type)_code_ptr[ip + 3];
				GD_ERR_BREAK(builtin_type < 0 || builtin_type >= Variant::VARIANT_MAX);

				*dst = value->get_type() == builtin_type;
				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_TYPE_TEST_ARRAY) {
				CHECK_SPACE(6);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(value, 1);

				GET_VARIANT_PTR(script_type, 2);
				Variant::Type builtin_type = (Variant::Type)_code_ptr[ip + 4];
				int native_type_idx = _code_ptr[ip + 5];
				GD_ERR_BREAK(native_type_idx < 0 || native_type_idx >= _global_names_count);
				const StringName native_type = _global_names_ptr[native_type_idx];

				bool result = false;
				if (value->get_type() == Variant::ARRAY) {
					Array *array = VariantInternal::get_array(value);
					result = array->get_typed_builtin() == ((uint32_t)builtin_type) && array->get_typed_class_name() == native_type && array->get_typed_script() == *script_type;
				}

				*dst = result;
				ip += 6;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_TYPE_TEST_DICTIONARY) {
				CHECK_SPACE(9);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(value, 1);

				GET_VARIANT_PTR(key_script_type, 2);
				Variant::Type key_builtin_type = (Variant::Type)_code_ptr[ip + 5];
				int key_native_type_idx = _code_ptr[ip + 6];
				GD_ERR_BREAK(key_native_type_idx < 0 || key_native_type_idx >= _global_names_count);
				const StringName key_native_type = _global_names_ptr[key_native_type_idx];

				GET_VARIANT_PTR(value_script_type, 3);
				Variant::Type value_builtin_type = (Variant::Type)_code_ptr[ip + 7];
				int value_native_type_idx = _code_ptr[ip + 8];
				GD_ERR_BREAK(value_native_type_idx < 0 || value_native_type_idx >= _global_names_count);
				const StringName value_native_type = _global_names_ptr[value_native_type_idx];

				bool result = false;
				if (value->get_type() == Variant::DICTIONARY) {
					Dictionary *dictionary = VariantInternal::get_dictionary(value);
					result = dictionary->get_typed_key_builtin() == ((uint32_t)key_builtin_type) && dictionary->get_typed_key_class_name() == key_native_type && dictionary->get_typed_key_script() == *key_script_type &&
							dictionary->get_typed_value_builtin() == ((uint32_t)value_builtin_type) && dictionary->get_typed_value_class_name() == value_native_type && dictionary->get_typed_value_script() == *value_script_type;
				}

				*dst = result;
				ip += 9;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_TYPE_TEST_NATIVE) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(value, 1);

				int native_type_idx = _code_ptr[ip + 3];
				GD_ERR_BREAK(native_type_idx < 0 || native_type_idx >= _global_names_count);
				const StringName native_type = _global_names_ptr[native_type_idx];

				bool was_freed = false;
				Object *object = value->get_validated_object_with_check(was_freed);
				if (was_freed) {
					err_text = "Left operand of 'is' is a previously freed instance.";
					OPCODE_BREAK;
				}

				*dst = object && ClassDB::is_parent_class(object->get_class_name(), native_type);
				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_TYPE_TEST_SCRIPT) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(value, 1);

				GET_VARIANT_PTR(type, 2);
				Script *script_type = Object::cast_to<Script>(type->operator Object *());
				GD_ERR_BREAK(!script_type);

				bool was_freed = false;
				Object *object = value->get_validated_object_with_check(was_freed);
				if (was_freed) {
					err_text = "Left operand of 'is' is a previously freed instance.";
					OPCODE_BREAK;
				}

				bool result = false;
				if (object && object->get_script_instance()) {
					Script *script_ptr = object->get_script_instance()->get_script().ptr();
					while (script_ptr) {
						if (script_ptr == script_type) {
							result = true;
							break;
						}
						script_ptr = script_ptr->get_base_script().ptr();
					}
				}

				*dst = result;
				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_SET_KEYED) {
				CHECK_SPACE(3);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(index, 1);
				GET_VARIANT_PTR(value, 2);

				bool valid;
#ifdef DEBUG_ENABLED
				Variant::VariantSetError err_code;
				dst->set(*index, *value, &valid, &err_code);
#else
				dst->set(*index, *value, &valid);
#endif
#ifdef DEBUG_ENABLED
				if (!valid) {
					if (dst->is_read_only()) {
						err_text = "Invalid assignment on read-only value (on base: '" + _get_var_type(dst) + "').";
					} else {
						Object *obj = dst->get_validated_object();
						String v = index->operator String();
						bool read_only_property = false;
						if (obj) {
							read_only_property = ClassDB::has_property(obj->get_class_name(), v) && (ClassDB::get_property_setter(obj->get_class_name(), v) == StringName());
						}
						if (read_only_property) {
							err_text = vformat(R"(Cannot set value into property "%s" (on base "%s") because it is read-only.)", v, _get_var_type(dst));
						} else {
							if (!v.is_empty()) {
								v = "'" + v + "'";
							} else {
								v = "of type '" + _get_var_type(index) + "'";
							}
							err_text = "Invalid assignment of property or key " + v + " with value of type '" + _get_var_type(value) + "' on a base object of type '" + _get_var_type(dst) + "'.";
							if (err_code == Variant::VariantSetError::SET_INDEXED_ERR) {
								err_text = "Invalid assignment of index " + v + " (on base: '" + _get_var_type(dst) + "') with value of type '" + _get_var_type(value) + "'.";
							}
						}
					}
					OPCODE_BREAK;
				}
#endif
				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_SET_KEYED_VALIDATED) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(index, 1);
				GET_VARIANT_PTR(value, 2);

				int index_setter = _code_ptr[ip + 4];
				GD_ERR_BREAK(index_setter < 0 || index_setter >= _keyed_setters_count);
				const Variant::ValidatedKeyedSetter setter = _keyed_setters_ptr[index_setter];

				bool valid;
				setter(dst, index, value, &valid);

#ifdef DEBUG_ENABLED
				if (!valid) {
					if (dst->is_read_only()) {
						err_text = "Invalid assignment on read-only value (on base: '" + _get_var_type(dst) + "').";
					} else {
						String v = index->operator String();
						if (!v.is_empty()) {
							v = "'" + v + "'";
						} else {
							v = "of type '" + _get_var_type(index) + "'";
						}
						err_text = "Invalid assignment of property or key " + v + " with value of type '" + _get_var_type(value) + "' on a base object of type '" + _get_var_type(dst) + "'.";
					}
					OPCODE_BREAK;
				}
#endif
				ip += 5;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_SET_INDEXED_VALIDATED) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(index, 1);
				GET_VARIANT_PTR(value, 2);

				int index_setter = _code_ptr[ip + 4];
				GD_ERR_BREAK(index_setter < 0 || index_setter >= _indexed_setters_count);
				const Variant::ValidatedIndexedSetter setter = _indexed_setters_ptr[index_setter];

				int64_t int_index = *VariantInternal::get_int(index);

				bool oob;
				setter(dst, int_index, value, &oob);

#ifdef DEBUG_ENABLED
				if (oob) {
					if (dst->is_read_only()) {
						err_text = "Invalid assignment on read-only value (on base: '" + _get_var_type(dst) + "').";
					} else {
						String v = index->operator String();
						if (!v.is_empty()) {
							v = "'" + v + "'";
						} else {
							v = "of type '" + _get_var_type(index) + "'";
						}
						err_text = "Out of bounds set index " + v + " (on base: '" + _get_var_type(dst) + "')";
					}
					OPCODE_BREAK;
				}
#endif
				ip += 5;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_GET_KEYED) {
				CHECK_SPACE(3);

				GET_VARIANT_PTR(src, 0);
				GET_VARIANT_PTR(index, 1);
				GET_VARIANT_PTR(dst, 2);

				bool valid;
#ifdef DEBUG_ENABLED
				// Allow better error message in cases where src and dst are the same stack position.
				Variant::VariantGetError err_code;
				Variant ret = src->get(*index, &valid, &err_code);
#else
				*dst = src->get(*index, &valid);

#endif
#ifdef DEBUG_ENABLED
				if (!valid) {
					String v = index->operator String();
					if (!v.is_empty()) {
						v = "'" + v + "'";
					} else {
						v = "of type '" + _get_var_type(index) + "'";
					}
					err_text = "Invalid access to property or key " + v + " on a base object of type '" + _get_var_type(src) + "'.";
					if (err_code == Variant::VariantGetError::GET_INDEXED_ERR) {
						err_text = "Invalid access of index " + v + " on a base object of type: '" + _get_var_type(src) + "'.";
					}
					OPCODE_BREAK;
				}
				*dst = ret;
#endif
				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_GET_KEYED_VALIDATED) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(src, 0);
				GET_VARIANT_PTR(key, 1);
				GET_VARIANT_PTR(dst, 2);

				int index_getter = _code_ptr[ip + 4];
				GD_ERR_BREAK(index_getter < 0 || index_getter >= _keyed_getters_count);
				const Variant::ValidatedKeyedGetter getter = _keyed_getters_ptr[index_getter];

				bool valid;
#ifdef DEBUG_ENABLED
				// Allow better error message in cases where src and dst are the same stack position.
				Variant ret;
				getter(src, key, &ret, &valid);
#else
				getter(src, key, dst, &valid);
#endif
#ifdef DEBUG_ENABLED
				if (!valid) {
					String v = key->operator String();
					if (!v.is_empty()) {
						v = "'" + v + "'";
					} else {
						v = "of type '" + _get_var_type(key) + "'";
					}
					err_text = "Invalid access to property or key " + v + " on a base object of type '" + _get_var_type(src) + "'.";
					OPCODE_BREAK;
				}
				*dst = ret;
#endif
				ip += 5;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_GET_INDEXED_VALIDATED) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(src, 0);
				GET_VARIANT_PTR(index, 1);
				GET_VARIANT_PTR(dst, 2);

				int index_getter = _code_ptr[ip + 4];
				GD_ERR_BREAK(index_getter < 0 || index_getter >= _indexed_getters_count);
				const Variant::ValidatedIndexedGetter getter = _indexed_getters_ptr[index_getter];

				int64_t int_index = *VariantInternal::get_int(index);

				bool oob;
				getter(src, int_index, dst, &oob);

#ifdef DEBUG_ENABLED
				if (oob) {
					String v = index->operator String();
					if (!v.is_empty()) {
						v = "'" + v + "'";
					} else {
						v = "of type '" + _get_var_type(index) + "'";
					}
					err_text = "Out of bounds get index " + v + " (on base: '" + _get_var_type(src) + "')";
					OPCODE_BREAK;
				}
#endif
				ip += 5;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_SET_NAMED) {
				CHECK_SPACE(3);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(value, 1);

				int indexname = _code_ptr[ip + 3];

				GD_ERR_BREAK(indexname < 0 || indexname >= _global_names_count);
				const StringName *index = &_global_names_ptr[indexname];

				bool valid;
				dst->set_named(*index, *value, valid);

#ifdef DEBUG_ENABLED
				if (!valid) {
					if (dst->is_read_only()) {
						err_text = "Invalid assignment on read-only value (on base: '" + _get_var_type(dst) + "').";
					} else {
						Object *obj = dst->get_validated_object();
						bool read_only_property = false;
						if (obj) {
							read_only_property = ClassDB::has_property(obj->get_class_name(), *index) && (ClassDB::get_property_setter(obj->get_class_name(), *index) == StringName());
						}
						if (read_only_property) {
							err_text = vformat(R"(Cannot set value into property "%s" (on base "%s") because it is read-only.)", String(*index), _get_var_type(dst));
						} else {
							err_text = "Invalid assignment of property or key '" + String(*index) + "' with value of type '" + _get_var_type(value) + "' on a base object of type '" + _get_var_type(dst) + "'.";
						}
					}
					OPCODE_BREAK;
				}
#endif
				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_SET_NAMED_VALIDATED) {
				CHECK_SPACE(3);

				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(value, 1);

				int index_setter = _code_ptr[ip + 3];
				GD_ERR_BREAK(index_setter < 0 || index_setter >= _setters_count);
				const Variant::ValidatedSetter setter = _setters_ptr[index_setter];

				setter(dst, value);
				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_GET_NAMED) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(src, 0);
				GET_VARIANT_PTR(dst, 1);

				int indexname = _code_ptr[ip + 3];

				GD_ERR_BREAK(indexname < 0 || indexname >= _global_names_count);
				const StringName *index = &_global_names_ptr[indexname];

				bool valid;
#ifdef DEBUG_ENABLED
				//allow better error message in cases where src and dst are the same stack position
				Variant ret = src->get_named(*index, valid);

#else
				*dst = src->get_named(*index, valid);
#endif
#ifdef DEBUG_ENABLED
				if (!valid) {
					err_text = "Invalid access to property or key '" + index->operator String() + "' on a base object of type '" + _get_var_type(src) + "'.";
					OPCODE_BREAK;
				}
				*dst = ret;
#endif
				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_GET_NAMED_VALIDATED) {
				CHECK_SPACE(3);

				GET_VARIANT_PTR(src, 0);
				GET_VARIANT_PTR(dst, 1);

				int index_getter = _code_ptr[ip + 3];
				GD_ERR_BREAK(index_getter < 0 || index_getter >= _getters_count);
				const Variant::ValidatedGetter getter = _getters_ptr[index_getter];

				getter(src, dst);
				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_SET_MEMBER) {
				CHECK_SPACE(3);
				GET_VARIANT_PTR(src, 0);
				int indexname = _code_ptr[ip + 2];
				GD_ERR_BREAK(indexname < 0 || indexname >= _global_names_count);
				const StringName *index = &_global_names_ptr[indexname];

				bool valid;
#ifndef DEBUG_ENABLED
				ClassDB::set_property(p_instance->owner, *index, *src, &valid);
#else
				bool ok = ClassDB::set_property(p_instance->owner, *index, *src, &valid);
				if (!ok) {
					err_text = "Internal error setting property: " + String(*index);
					OPCODE_BREAK;
				} else if (!valid) {
					err_text = "Error setting property '" + String(*index) + "' with value of type " + Variant::get_type_name(src->get_type()) + ".";
					OPCODE_BREAK;
				}
#endif
				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_GET_MEMBER) {
				CHECK_SPACE(3);
				GET_VARIANT_PTR(dst, 0);
				int indexname = _code_ptr[ip + 2];
				GD_ERR_BREAK(indexname < 0 || indexname >= _global_names_count);
				const StringName *index = &_global_names_ptr[indexname];
#ifndef DEBUG_ENABLED
				ClassDB::get_property(p_instance->owner, *index, *dst);
#else
				bool ok = ClassDB::get_property(p_instance->owner, *index, *dst);
				if (!ok) {
					err_text = "Internal error getting property: " + String(*index);
					OPCODE_BREAK;
				}
#endif
				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_SET_STATIC_VARIABLE) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(value, 0);

				GET_VARIANT_PTR(_class, 1);
				GDScript *gdscript = Object::cast_to<GDScript>(_class->operator Object *());
				GD_ERR_BREAK(!gdscript);

				int index = _code_ptr[ip + 3];
				GD_ERR_BREAK(index < 0 || index >= gdscript->static_variables.size());

				gdscript->static_variables.write[index] = *value;

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_GET_STATIC_VARIABLE) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(target, 0);

				GET_VARIANT_PTR(_class, 1);
				GDScript *gdscript = Object::cast_to<GDScript>(_class->operator Object *());
				GD_ERR_BREAK(!gdscript);

				int index = _code_ptr[ip + 3];
				GD_ERR_BREAK(index < 0 || index >= gdscript->static_variables.size());

				*target = gdscript->static_variables[index];

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ASSIGN) {
				CHECK_SPACE(3);
				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(src, 1);

				*dst = *src;

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ASSIGN_NULL) {
				CHECK_SPACE(2);
				GET_VARIANT_PTR(dst, 0);

				*dst = Variant();

				ip += 2;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ASSIGN_TRUE) {
				CHECK_SPACE(2);
				GET_VARIANT_PTR(dst, 0);

				*dst = true;

				ip += 2;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ASSIGN_FALSE) {
				CHECK_SPACE(2);
				GET_VARIANT_PTR(dst, 0);

				*dst = false;

				ip += 2;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ASSIGN_TYPED_BUILTIN) {
				CHECK_SPACE(4);
				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(src, 1);

				Variant::Type var_type = (Variant::Type)_code_ptr[ip + 3];
				GD_ERR_BREAK(var_type < 0 || var_type >= Variant::VARIANT_MAX);

				if (src->get_type() != var_type) {
#ifdef DEBUG_ENABLED
					if (Variant::can_convert_strict(src->get_type(), var_type)) {
#endif // DEBUG_ENABLED
						Callable::CallError ce;
						Variant::construct(var_type, *dst, const_cast<const Variant **>(&src), 1, ce);
					} else {
#ifdef DEBUG_ENABLED
						err_text = "Trying to assign value of type '" + Variant::get_type_name(src->get_type()) +
								"' to a variable of type '" + Variant::get_type_name(var_type) + "'.";
						OPCODE_BREAK;
					}
				} else {
#endif // DEBUG_ENABLED
					*dst = *src;
				}

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ASSIGN_TYPED_ARRAY) {
				CHECK_SPACE(6);
				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(src, 1);

				GET_VARIANT_PTR(script_type, 2);
				Variant::Type builtin_type = (Variant::Type)_code_ptr[ip + 4];
				int native_type_idx = _code_ptr[ip + 5];
				GD_ERR_BREAK(native_type_idx < 0 || native_type_idx >= _global_names_count);
				const StringName native_type = _global_names_ptr[native_type_idx];

				if (src->get_type() != Variant::ARRAY) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to assign a value of type "%s" to a variable of type "Array[%s]".)",
							_get_var_type(src), _get_element_type(builtin_type, native_type, *script_type));
#endif // DEBUG_ENABLED
					OPCODE_BREAK;
				}

				Array *array = VariantInternal::get_array(src);

				if (array->get_typed_builtin() != ((uint32_t)builtin_type) || array->get_typed_class_name() != native_type || array->get_typed_script() != *script_type) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to assign an array of type "%s" to a variable of type "Array[%s]".)",
							_get_var_type(src), _get_element_type(builtin_type, native_type, *script_type));
#endif // DEBUG_ENABLED
					OPCODE_BREAK;
				}

				*dst = *src;

				ip += 6;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ASSIGN_TYPED_DICTIONARY) {
				CHECK_SPACE(9);
				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(src, 1);

				GET_VARIANT_PTR(key_script_type, 2);
				Variant::Type key_builtin_type = (Variant::Type)_code_ptr[ip + 5];
				int key_native_type_idx = _code_ptr[ip + 6];
				GD_ERR_BREAK(key_native_type_idx < 0 || key_native_type_idx >= _global_names_count);
				const StringName key_native_type = _global_names_ptr[key_native_type_idx];

				GET_VARIANT_PTR(value_script_type, 3);
				Variant::Type value_builtin_type = (Variant::Type)_code_ptr[ip + 7];
				int value_native_type_idx = _code_ptr[ip + 8];
				GD_ERR_BREAK(value_native_type_idx < 0 || value_native_type_idx >= _global_names_count);
				const StringName value_native_type = _global_names_ptr[value_native_type_idx];

				if (src->get_type() != Variant::DICTIONARY) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to assign a value of type "%s" to a variable of type "Dictionary[%s, %s]".)",
							_get_var_type(src), _get_element_type(key_builtin_type, key_native_type, *key_script_type),
							_get_element_type(value_builtin_type, value_native_type, *value_script_type));
#endif // DEBUG_ENABLED
					OPCODE_BREAK;
				}

				Dictionary *dictionary = VariantInternal::get_dictionary(src);

				if (dictionary->get_typed_key_builtin() != ((uint32_t)key_builtin_type) || dictionary->get_typed_key_class_name() != key_native_type || dictionary->get_typed_key_script() != *key_script_type ||
						dictionary->get_typed_value_builtin() != ((uint32_t)value_builtin_type) || dictionary->get_typed_value_class_name() != value_native_type || dictionary->get_typed_value_script() != *value_script_type) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to assign a dictionary of type "%s" to a variable of type "Dictionary[%s, %s]".)",
							_get_var_type(src), _get_element_type(key_builtin_type, key_native_type, *key_script_type),
							_get_element_type(value_builtin_type, value_native_type, *value_script_type));
#endif // DEBUG_ENABLED
					OPCODE_BREAK;
				}

				*dst = *src;

				ip += 9;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ASSIGN_TYPED_NATIVE) {
				CHECK_SPACE(4);
				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(src, 1);

#ifdef DEBUG_ENABLED
				GET_VARIANT_PTR(type, 2);
				GDScriptNativeClass *nc = Object::cast_to<GDScriptNativeClass>(type->operator Object *());
				GD_ERR_BREAK(!nc);
				if (src->get_type() != Variant::OBJECT && src->get_type() != Variant::NIL) {
					err_text = "Trying to assign value of type '" + Variant::get_type_name(src->get_type()) +
							"' to a variable of type '" + nc->get_name() + "'.";
					OPCODE_BREAK;
				}

				if (src->get_type() == Variant::OBJECT) {
					bool was_freed = false;
					Object *src_obj = src->get_validated_object_with_check(was_freed);
					if (!src_obj && was_freed) {
						err_text = "Trying to assign invalid previously freed instance.";
						OPCODE_BREAK;
					}

					if (src_obj && !ClassDB::is_parent_class(src_obj->get_class_name(), nc->get_name())) {
						err_text = "Trying to assign value of type '" + src_obj->get_class_name() +
								"' to a variable of type '" + nc->get_name() + "'.";
						OPCODE_BREAK;
					}
				}
#endif // DEBUG_ENABLED
				*dst = *src;

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ASSIGN_TYPED_SCRIPT) {
				CHECK_SPACE(4);
				GET_VARIANT_PTR(dst, 0);
				GET_VARIANT_PTR(src, 1);

#ifdef DEBUG_ENABLED
				GET_VARIANT_PTR(type, 2);
				Script *base_type = Object::cast_to<Script>(type->operator Object *());

				GD_ERR_BREAK(!base_type);

				if (src->get_type() != Variant::OBJECT && src->get_type() != Variant::NIL) {
					err_text = "Trying to assign a non-object value to a variable of type '" + base_type->get_path().get_file() + "'.";
					OPCODE_BREAK;
				}

				if (src->get_type() == Variant::OBJECT) {
					bool was_freed = false;
					Object *val_obj = src->get_validated_object_with_check(was_freed);
					if (!val_obj && was_freed) {
						err_text = "Trying to assign invalid previously freed instance.";
						OPCODE_BREAK;
					}

					if (val_obj) { // src is not null
						ScriptInstance *scr_inst = val_obj->get_script_instance();
						if (!scr_inst) {
							err_text = "Trying to assign value of type '" + val_obj->get_class_name() +
									"' to a variable of type '" + base_type->get_path().get_file() + "'.";
							OPCODE_BREAK;
						}

						Script *src_type = scr_inst->get_script().ptr();
						bool valid = false;

						while (src_type) {
							if (src_type == base_type) {
								valid = true;
								break;
							}
							src_type = src_type->get_base_script().ptr();
						}

						if (!valid) {
							err_text = "Trying to assign value of type '" + val_obj->get_script_instance()->get_script()->get_path().get_file() +
									"' to a variable of type '" + base_type->get_path().get_file() + "'.";
							OPCODE_BREAK;
						}
					}
				}
#endif // DEBUG_ENABLED

				*dst = *src;

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CAST_TO_BUILTIN) {
				CHECK_SPACE(4);
				GET_VARIANT_PTR(src, 0);
				GET_VARIANT_PTR(dst, 1);
				Variant::Type to_type = (Variant::Type)_code_ptr[ip + 3];

				GD_ERR_BREAK(to_type < 0 || to_type >= Variant::VARIANT_MAX);

#ifdef DEBUG_ENABLED
				if (src->operator Object *() && !src->get_validated_object()) {
					err_text = "Trying to cast a freed object.";
					OPCODE_BREAK;
				}
#endif

				Callable::CallError err;
				Variant::construct(to_type, *dst, (const Variant **)&src, 1, err);

#ifdef DEBUG_ENABLED
				if (err.error != Callable::CallError::CALL_OK) {
					err_text = "Invalid cast: could not convert value to '" + Variant::get_type_name(to_type) + "'.";
					OPCODE_BREAK;
				}
#endif

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CAST_TO_NATIVE) {
				CHECK_SPACE(4);
				GET_VARIANT_PTR(src, 0);
				GET_VARIANT_PTR(dst, 1);
				GET_VARIANT_PTR(to_type, 2);

				GDScriptNativeClass *nc = Object::cast_to<GDScriptNativeClass>(to_type->operator Object *());
				GD_ERR_BREAK(!nc);

#ifdef DEBUG_ENABLED
				if (src->operator Object *() && !src->get_validated_object()) {
					err_text = "Trying to cast a freed object.";
					OPCODE_BREAK;
				}
				if (src->get_type() != Variant::OBJECT && src->get_type() != Variant::NIL) {
					err_text = "Invalid cast: can't convert a non-object value to an object type.";
					OPCODE_BREAK;
				}
#endif
				Object *src_obj = src->operator Object *();

				if (src_obj && !ClassDB::is_parent_class(src_obj->get_class_name(), nc->get_name())) {
					*dst = Variant(); // invalid cast, assign NULL
				} else {
					*dst = *src;
				}

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CAST_TO_SCRIPT) {
				CHECK_SPACE(4);
				GET_VARIANT_PTR(src, 0);
				GET_VARIANT_PTR(dst, 1);
				GET_VARIANT_PTR(to_type, 2);

				Script *base_type = Object::cast_to<Script>(to_type->operator Object *());

				GD_ERR_BREAK(!base_type);

#ifdef DEBUG_ENABLED
				if (src->operator Object *() && !src->get_validated_object()) {
					err_text = "Trying to cast a freed object.";
					OPCODE_BREAK;
				}
				if (src->get_type() != Variant::OBJECT && src->get_type() != Variant::NIL) {
					err_text = "Trying to assign a non-object value to a variable of type '" + base_type->get_path().get_file() + "'.";
					OPCODE_BREAK;
				}
#endif

				bool valid = false;

				if (src->get_type() != Variant::NIL && src->operator Object *() != nullptr) {
					ScriptInstance *scr_inst = src->operator Object *()->get_script_instance();

					if (scr_inst) {
						Script *src_type = src->operator Object *()->get_script_instance()->get_script().ptr();

						while (src_type) {
							if (src_type == base_type) {
								valid = true;
								break;
							}
							src_type = src_type->get_base_script().ptr();
						}
					}
				}

				if (valid) {
					*dst = *src; // Valid cast, copy the source object
				} else {
					*dst = Variant(); // invalid cast, assign NULL
				}

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CONSTRUCT) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(2 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];

				Variant::Type t = Variant::Type(_code_ptr[ip + 2]);

				Variant **argptrs = instruction_args;

				GET_INSTRUCTION_ARG(dst, argc);

				Callable::CallError err;
				Variant::construct(t, *dst, (const Variant **)argptrs, argc, err);

#ifdef DEBUG_ENABLED
				if (err.error != Callable::CallError::CALL_OK) {
					err_text = _get_call_error("'" + Variant::get_type_name(t) + "' constructor", (const Variant **)argptrs, *dst, err);
					OPCODE_BREAK;
				}
#endif

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CONSTRUCT_VALIDATED) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(2 + instr_arg_count);
				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];

				int constructor_idx = _code_ptr[ip + 2];
				GD_ERR_BREAK(constructor_idx < 0 || constructor_idx >= _constructors_count);
				Variant::ValidatedConstructor constructor = _constructors_ptr[constructor_idx];

				Variant **argptrs = instruction_args;

				GET_INSTRUCTION_ARG(dst, argc);

				constructor(dst, (const Variant **)argptrs);

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CONSTRUCT_ARRAY) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(1 + instr_arg_count);
				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				Array array;
				array.resize(argc);

				for (int i = 0; i < argc; i++) {
					array[i] = *(instruction_args[i]);
				}

				GET_INSTRUCTION_ARG(dst, argc);
				*dst = Variant(); // Clear potential previous typed array.

				*dst = array;

				ip += 2;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CONSTRUCT_TYPED_ARRAY) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);
				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];

				GET_INSTRUCTION_ARG(script_type, argc + 1);
				Variant::Type builtin_type = (Variant::Type)_code_ptr[ip + 2];
				int native_type_idx = _code_ptr[ip + 3];
				GD_ERR_BREAK(native_type_idx < 0 || native_type_idx >= _global_names_count);
				const StringName native_type = _global_names_ptr[native_type_idx];

				Array array;
				array.resize(argc);
				for (int i = 0; i < argc; i++) {
					array[i] = *(instruction_args[i]);
				}

				GET_INSTRUCTION_ARG(dst, argc);
				*dst = Variant(); // Clear potential previous typed array.

				*dst = Array(array, builtin_type, native_type, *script_type);

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CONSTRUCT_DICTIONARY) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(2 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				Dictionary dict;

				for (int i = 0; i < argc; i++) {
					GET_INSTRUCTION_ARG(k, i * 2 + 0);
					GET_INSTRUCTION_ARG(v, i * 2 + 1);
					dict[*k] = *v;
				}

				GET_INSTRUCTION_ARG(dst, argc * 2);

				*dst = Variant(); // Clear potential previous typed dictionary.

				*dst = dict;

				ip += 2;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CONSTRUCT_TYPED_DICTIONARY) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(6 + instr_arg_count);
				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];

				GET_INSTRUCTION_ARG(key_script_type, argc * 2 + 1);
				Variant::Type key_builtin_type = (Variant::Type)_code_ptr[ip + 2];
				int key_native_type_idx = _code_ptr[ip + 3];
				GD_ERR_BREAK(key_native_type_idx < 0 || key_native_type_idx >= _global_names_count);
				const StringName key_native_type = _global_names_ptr[key_native_type_idx];

				GET_INSTRUCTION_ARG(value_script_type, argc * 2 + 2);
				Variant::Type value_builtin_type = (Variant::Type)_code_ptr[ip + 4];
				int value_native_type_idx = _code_ptr[ip + 5];
				GD_ERR_BREAK(value_native_type_idx < 0 || value_native_type_idx >= _global_names_count);
				const StringName value_native_type = _global_names_ptr[value_native_type_idx];

				Dictionary dict;

				for (int i = 0; i < argc; i++) {
					GET_INSTRUCTION_ARG(k, i * 2 + 0);
					GET_INSTRUCTION_ARG(v, i * 2 + 1);
					dict[*k] = *v;
				}

				GET_INSTRUCTION_ARG(dst, argc * 2);

				*dst = Variant(); // Clear potential previous typed dictionary.

				*dst = Dictionary(dict, key_builtin_type, key_native_type, *key_script_type, value_builtin_type, value_native_type, *value_script_type);

				ip += 6;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_ASYNC)
			OPCODE(OPCODE_CALL_RETURN)
			OPCODE(OPCODE_CALL) {
				bool call_ret = (_code_ptr[ip]) != OPCODE_CALL;
#ifdef DEBUG_ENABLED
				bool call_async = (_code_ptr[ip]) == OPCODE_CALL_ASYNC;
#endif
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				int methodname_idx = _code_ptr[ip + 2];
				GD_ERR_BREAK(methodname_idx < 0 || methodname_idx >= _global_names_count);
				const StringName *methodname = &_global_names_ptr[methodname_idx];

				GET_INSTRUCTION_ARG(base, argc);
				Variant **argptrs = instruction_args;

#ifdef DEBUG_ENABLED
				uint64_t call_time = 0;

				if (GDScriptLanguage::get_singleton()->profiling) {
					call_time = OS::get_singleton()->get_ticks_usec();
				}
				Variant::Type base_type = base->get_type();
				Object *base_obj = base->get_validated_object();
				StringName base_class = base_obj ? base_obj->get_class_name() : StringName();
#endif

				Variant temp_ret;
				Callable::CallError err;
				if (call_ret) {
					GET_INSTRUCTION_ARG(ret, argc + 1);
					base->callp(*methodname, (const Variant **)argptrs, argc, temp_ret, err);
					*ret = temp_ret;
#ifdef DEBUG_ENABLED
					if (ret->get_type() == Variant::NIL) {
						if (base_type == Variant::OBJECT) {
							if (base_obj) {
								MethodBind *method = ClassDB::get_method(base_class, *methodname);
								if (*methodname == CoreStringName(free_) || (method && !method->has_return())) {
									err_text = R"(Trying to get a return value of a method that returns "void")";
									OPCODE_BREAK;
								}
							}
						} else if (Variant::has_builtin_method(base_type, *methodname) && !Variant::has_builtin_method_return_value(base_type, *methodname)) {
							err_text = R"(Trying to get a return value of a method that returns "void")";
							OPCODE_BREAK;
						}
					}

					if (!call_async && ret->get_type() == Variant::OBJECT) {
						// Check if getting a function state without await.
						bool was_freed = false;
						Object *obj = ret->get_validated_object_with_check(was_freed);

						if (obj && obj->is_class_ptr(GDScriptFunctionState::get_class_ptr_static())) {
							err_text = R"(Trying to call an async function without "await".)";
							OPCODE_BREAK;
						}
					}
#endif
				} else {
					base->callp(*methodname, (const Variant **)argptrs, argc, temp_ret, err);
				}
#ifdef DEBUG_ENABLED

				if (GDScriptLanguage::get_singleton()->profiling) {
					uint64_t t_taken = OS::get_singleton()->get_ticks_usec() - call_time;
					if (GDScriptLanguage::get_singleton()->profile_native_calls && _profile_count_as_native(base_obj, *methodname)) {
						_profile_native_call(t_taken, *methodname, base_class);
					}
					function_call_time += t_taken;
				}

				if (err.error != Callable::CallError::CALL_OK) {
					String methodstr = *methodname;
					String basestr = _get_var_type(base);
					bool is_callable = false;

					if (methodstr == "call") {
						if (argc >= 1 && base->get_type() != Variant::CALLABLE) {
							methodstr = String(*argptrs[0]) + " (via call)";
							if (err.error == Callable::CallError::CALL_ERROR_INVALID_ARGUMENT) {
								err.argument += 1;
							}
						} else {
							methodstr = base->operator String() + " (Callable)";
							is_callable = true;
						}
					} else if (methodstr == "free") {
						if (err.error == Callable::CallError::CALL_ERROR_INVALID_METHOD) {
							if (base->is_ref_counted()) {
								err_text = "Attempted to free a RefCounted object.";
								OPCODE_BREAK;
							} else if (base->get_type() == Variant::OBJECT) {
								err_text = "Attempted to free a locked object (calling or emitting).";
								OPCODE_BREAK;
							}
						}
					} else if (methodstr == "call_recursive" && basestr == "TreeItem") {
						if (argc >= 1) {
							methodstr = String(*argptrs[0]) + " (via TreeItem.call_recursive)";
							if (err.error == Callable::CallError::CALL_ERROR_INVALID_ARGUMENT) {
								err.argument += 1;
							}
						}
					}
					err_text = _get_call_error("function '" + methodstr + (is_callable ? "" : "' in base '" + basestr) + "'", (const Variant **)argptrs, temp_ret, err);
					OPCODE_BREAK;
				}
#endif // DEBUG_ENABLED

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_METHOD_BIND)
			OPCODE(OPCODE_CALL_METHOD_BIND_RET) {
				bool call_ret = (_code_ptr[ip]) == OPCODE_CALL_METHOD_BIND_RET;
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);
				GD_ERR_BREAK(_code_ptr[ip + 2] < 0 || _code_ptr[ip + 2] >= _methods_count);
				MethodBind *method = _methods_ptr[_code_ptr[ip + 2]];

				GET_INSTRUCTION_ARG(base, argc);

#ifdef DEBUG_ENABLED
				bool freed = false;
				Object *base_obj = base->get_validated_object_with_check(freed);
				if (freed) {
					err_text = METHOD_CALL_ON_FREED_INSTANCE_ERROR(method);
					OPCODE_BREAK;
				} else if (!base_obj) {
					err_text = METHOD_CALL_ON_NULL_VALUE_ERROR(method);
					OPCODE_BREAK;
				}
#else
				Object *base_obj = base->operator Object *();
#endif
				Variant **argptrs = instruction_args;

#ifdef DEBUG_ENABLED
				uint64_t call_time = 0;
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					call_time = OS::get_singleton()->get_ticks_usec();
				}
#endif

				Variant temp_ret;
				Callable::CallError err;
				if (call_ret) {
					GET_INSTRUCTION_ARG(ret, argc + 1);
					temp_ret = method->call(base_obj, (const Variant **)argptrs, argc, err);
					*ret = temp_ret;
				} else {
					temp_ret = method->call(base_obj, (const Variant **)argptrs, argc, err);
				}

#ifdef DEBUG_ENABLED

				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					uint64_t t_taken = OS::get_singleton()->get_ticks_usec() - call_time;
					_profile_native_call(t_taken, method->get_name(), method->get_instance_class());
					function_call_time += t_taken;
				}

				if (err.error != Callable::CallError::CALL_OK) {
					String methodstr = method->get_name();
					String basestr = _get_var_type(base);

					if (methodstr == "call") {
						if (argc >= 1) {
							methodstr = String(*argptrs[0]) + " (via call)";
							if (err.error == Callable::CallError::CALL_ERROR_INVALID_ARGUMENT) {
								err.argument += 1;
							}
						}
					} else if (methodstr == "free") {
						if (err.error == Callable::CallError::CALL_ERROR_INVALID_METHOD) {
							if (base->is_ref_counted()) {
								err_text = "Attempted to free a RefCounted object.";
								OPCODE_BREAK;
							} else if (base->get_type() == Variant::OBJECT) {
								err_text = "Attempted to free a locked object (calling or emitting).";
								OPCODE_BREAK;
							}
						}
					}
					err_text = _get_call_error("function '" + methodstr + "' in base '" + basestr + "'", (const Variant **)argptrs, temp_ret, err);
					OPCODE_BREAK;
				}
#endif
				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_BUILTIN_STATIC) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(4 + instr_arg_count);

				ip += instr_arg_count;

				GD_ERR_BREAK(_code_ptr[ip + 1] < 0 || _code_ptr[ip + 1] >= Variant::VARIANT_MAX);
				Variant::Type builtin_type = (Variant::Type)_code_ptr[ip + 1];

				int methodname_idx = _code_ptr[ip + 2];
				GD_ERR_BREAK(methodname_idx < 0 || methodname_idx >= _global_names_count);
				const StringName *methodname = &_global_names_ptr[methodname_idx];

				int argc = _code_ptr[ip + 3];
				GD_ERR_BREAK(argc < 0);

				GET_INSTRUCTION_ARG(ret, argc);

				const Variant **argptrs = const_cast<const Variant **>(instruction_args);

				Callable::CallError err;
				Variant::call_static(builtin_type, *methodname, argptrs, argc, *ret, err);

#ifdef DEBUG_ENABLED
				if (err.error != Callable::CallError::CALL_OK) {
					err_text = _get_call_error("static function '" + methodname->operator String() + "' in type '" + Variant::get_type_name(builtin_type) + "'", argptrs, *ret, err);
					OPCODE_BREAK;
				}
#endif

				ip += 4;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_NATIVE_STATIC) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				GD_ERR_BREAK(_code_ptr[ip + 1] < 0 || _code_ptr[ip + 1] >= _methods_count);
				MethodBind *method = _methods_ptr[_code_ptr[ip + 1]];

				int argc = _code_ptr[ip + 2];
				GD_ERR_BREAK(argc < 0);

				GET_INSTRUCTION_ARG(ret, argc);

				const Variant **argptrs = const_cast<const Variant **>(instruction_args);

#ifdef DEBUG_ENABLED
				uint64_t call_time = 0;
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					call_time = OS::get_singleton()->get_ticks_usec();
				}
#endif

				Callable::CallError err;
				*ret = method->call(nullptr, argptrs, argc, err);

#ifdef DEBUG_ENABLED
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					uint64_t t_taken = OS::get_singleton()->get_ticks_usec() - call_time;
					_profile_native_call(t_taken, method->get_name(), method->get_instance_class());
					function_call_time += t_taken;
				}
#endif

				if (err.error != Callable::CallError::CALL_OK) {
					err_text = _get_call_error("static function '" + method->get_name().operator String() + "' in type '" + method->get_instance_class().operator String() + "'", argptrs, *ret, err);
					OPCODE_BREAK;
				}

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_NATIVE_STATIC_VALIDATED_RETURN) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				GD_ERR_BREAK(_code_ptr[ip + 2] < 0 || _code_ptr[ip + 2] >= _methods_count);
				MethodBind *method = _methods_ptr[_code_ptr[ip + 2]];

				Variant **argptrs = instruction_args;

#ifdef DEBUG_ENABLED
				uint64_t call_time = 0;
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					call_time = OS::get_singleton()->get_ticks_usec();
				}
#endif

				GET_INSTRUCTION_ARG(ret, argc);
				method->validated_call(nullptr, (const Variant **)argptrs, ret);

#ifdef DEBUG_ENABLED
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					uint64_t t_taken = OS::get_singleton()->get_ticks_usec() - call_time;
					_profile_native_call(t_taken, method->get_name(), method->get_instance_class());
					function_call_time += t_taken;
				}
#endif

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				GD_ERR_BREAK(_code_ptr[ip + 2] < 0 || _code_ptr[ip + 2] >= _methods_count);
				MethodBind *method = _methods_ptr[_code_ptr[ip + 2]];

				Variant **argptrs = instruction_args;
#ifdef DEBUG_ENABLED
				uint64_t call_time = 0;
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					call_time = OS::get_singleton()->get_ticks_usec();
				}
#endif

				GET_INSTRUCTION_ARG(ret, argc);
				VariantInternal::initialize(ret, Variant::NIL);
				method->validated_call(nullptr, (const Variant **)argptrs, nullptr);

#ifdef DEBUG_ENABLED
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					uint64_t t_taken = OS::get_singleton()->get_ticks_usec() - call_time;
					_profile_native_call(t_taken, method->get_name(), method->get_instance_class());
					function_call_time += t_taken;
				}
#endif

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_METHOD_BIND_VALIDATED_RETURN) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				GD_ERR_BREAK(_code_ptr[ip + 2] < 0 || _code_ptr[ip + 2] >= _methods_count);
				MethodBind *method = _methods_ptr[_code_ptr[ip + 2]];

				GET_INSTRUCTION_ARG(base, argc);

#ifdef DEBUG_ENABLED
				bool freed = false;
				Object *base_obj = base->get_validated_object_with_check(freed);
				if (freed) {
					err_text = METHOD_CALL_ON_FREED_INSTANCE_ERROR(method);
					OPCODE_BREAK;
				} else if (!base_obj) {
					err_text = METHOD_CALL_ON_NULL_VALUE_ERROR(method);
					OPCODE_BREAK;
				}
#else
				Object *base_obj = *VariantInternal::get_object(base);
#endif

				Variant **argptrs = instruction_args;

#ifdef DEBUG_ENABLED
				uint64_t call_time = 0;
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					call_time = OS::get_singleton()->get_ticks_usec();
				}
#endif

				GET_INSTRUCTION_ARG(ret, argc + 1);
				method->validated_call(base_obj, (const Variant **)argptrs, ret);

#ifdef DEBUG_ENABLED
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					uint64_t t_taken = OS::get_singleton()->get_ticks_usec() - call_time;
					_profile_native_call(t_taken, method->get_name(), method->get_instance_class());
					function_call_time += t_taken;
				}
#endif

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_METHOD_BIND_VALIDATED_NO_RETURN) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				GD_ERR_BREAK(_code_ptr[ip + 2] < 0 || _code_ptr[ip + 2] >= _methods_count);
				MethodBind *method = _methods_ptr[_code_ptr[ip + 2]];

				GET_INSTRUCTION_ARG(base, argc);
#ifdef DEBUG_ENABLED
				bool freed = false;
				Object *base_obj = base->get_validated_object_with_check(freed);
				if (freed) {
					err_text = METHOD_CALL_ON_FREED_INSTANCE_ERROR(method);
					OPCODE_BREAK;
				} else if (!base_obj) {
					err_text = METHOD_CALL_ON_NULL_VALUE_ERROR(method);
					OPCODE_BREAK;
				}
#else
				Object *base_obj = *VariantInternal::get_object(base);
#endif
				Variant **argptrs = instruction_args;
#ifdef DEBUG_ENABLED
				uint64_t call_time = 0;
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					call_time = OS::get_singleton()->get_ticks_usec();
				}
#endif

				GET_INSTRUCTION_ARG(ret, argc + 1);
				VariantInternal::initialize(ret, Variant::NIL);
				method->validated_call(base_obj, (const Variant **)argptrs, nullptr);

#ifdef DEBUG_ENABLED
				if (GDScriptLanguage::get_singleton()->profiling && GDScriptLanguage::get_singleton()->profile_native_calls) {
					uint64_t t_taken = OS::get_singleton()->get_ticks_usec() - call_time;
					_profile_native_call(t_taken, method->get_name(), method->get_instance_class());
					function_call_time += t_taken;
				}
#endif

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_BUILTIN_TYPE_VALIDATED) {
				LOAD_INSTRUCTION_ARGS

				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				GET_INSTRUCTION_ARG(base, argc);

				GD_ERR_BREAK(_code_ptr[ip + 2] < 0 || _code_ptr[ip + 2] >= _builtin_methods_count);
				Variant::ValidatedBuiltInMethod method = _builtin_methods_ptr[_code_ptr[ip + 2]];
				Variant **argptrs = instruction_args;

				GET_INSTRUCTION_ARG(ret, argc + 1);
				method(base, (const Variant **)argptrs, argc, ret);

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_UTILITY) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				GD_ERR_BREAK(_code_ptr[ip + 2] < 0 || _code_ptr[ip + 2] >= _global_names_count);
				StringName function = _global_names_ptr[_code_ptr[ip + 2]];

				Variant **argptrs = instruction_args;

				GET_INSTRUCTION_ARG(dst, argc);

				Callable::CallError err;
				Variant::call_utility_function(function, dst, (const Variant **)argptrs, argc, err);

#ifdef DEBUG_ENABLED
				if (err.error != Callable::CallError::CALL_OK) {
					String methodstr = function;
					if (dst->get_type() == Variant::STRING && !dst->operator String().is_empty()) {
						// Call provided error string.
						err_text = vformat(R"*(Error calling utility function "%s()": %s)*", methodstr, *dst);
					} else {
						err_text = _get_call_error(vformat(R"*(utility function "%s()")*", methodstr), (const Variant **)argptrs, *dst, err);
					}
					OPCODE_BREAK;
				}
#endif
				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_UTILITY_VALIDATED) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				GD_ERR_BREAK(_code_ptr[ip + 2] < 0 || _code_ptr[ip + 2] >= _utilities_count);
				Variant::ValidatedUtilityFunction function = _utilities_ptr[_code_ptr[ip + 2]];

				Variant **argptrs = instruction_args;

				GET_INSTRUCTION_ARG(dst, argc);

				function(dst, (const Variant **)argptrs, argc);

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_GDSCRIPT_UTILITY) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				GD_ERR_BREAK(_code_ptr[ip + 2] < 0 || _code_ptr[ip + 2] >= _gds_utilities_count);
				GDScriptUtilityFunctions::FunctionPtr function = _gds_utilities_ptr[_code_ptr[ip + 2]];

				Variant **argptrs = instruction_args;

				GET_INSTRUCTION_ARG(dst, argc);

				Callable::CallError err;
				function(dst, (const Variant **)argptrs, argc, err);

#ifdef DEBUG_ENABLED
				if (err.error != Callable::CallError::CALL_OK) {
					String methodstr = gds_utilities_names[_code_ptr[ip + 2]];
					if (dst->get_type() == Variant::STRING && !dst->operator String().is_empty()) {
						// Call provided error string.
						err_text = vformat(R"*(Error calling GDScript utility function "%s()": %s)*", methodstr, *dst);
					} else {
						err_text = _get_call_error(vformat(R"*(GDScript utility function "%s()")*", methodstr), (const Variant **)argptrs, *dst, err);
					}
					OPCODE_BREAK;
				}
#endif
				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CALL_SELF_BASE) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(3 + instr_arg_count);

				ip += instr_arg_count;

				int argc = _code_ptr[ip + 1];
				GD_ERR_BREAK(argc < 0);

				int self_fun = _code_ptr[ip + 2];
#ifdef DEBUG_ENABLED
				if (self_fun < 0 || self_fun >= _global_names_count) {
					err_text = "compiler bug, function name not found";
					OPCODE_BREAK;
				}
#endif
				const StringName *methodname = &_global_names_ptr[self_fun];

				Variant **argptrs = instruction_args;

				GET_INSTRUCTION_ARG(dst, argc);

				const GDScript *gds = _script;

				HashMap<StringName, GDScriptFunction *>::ConstIterator E;
				while (gds->base.ptr()) {
					gds = gds->base.ptr();
					E = gds->member_functions.find(*methodname);
					if (E) {
						break;
					}
				}

				Callable::CallError err;

				if (E) {
					*dst = E->value->call(p_instance, (const Variant **)argptrs, argc, err);
				} else if (gds->native.ptr()) {
					if (*methodname != GDScriptLanguage::get_singleton()->strings._init) {
						MethodBind *mb = ClassDB::get_method(gds->native->get_name(), *methodname);
						if (!mb) {
							err.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
						} else {
							*dst = mb->call(p_instance->owner, (const Variant **)argptrs, argc, err);
						}
					} else {
						err.error = Callable::CallError::CALL_OK;
					}
				} else {
					if (*methodname != GDScriptLanguage::get_singleton()->strings._init) {
						err.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
					} else {
						err.error = Callable::CallError::CALL_OK;
					}
				}

				if (err.error != Callable::CallError::CALL_OK) {
					String methodstr = *methodname;
					err_text = _get_call_error("function '" + methodstr + "'", (const Variant **)argptrs, *dst, err);

					OPCODE_BREAK;
				}

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_AWAIT) {
				CHECK_SPACE(2);

				// Do the one-shot connect.
				GET_VARIANT_PTR(argobj, 0);

				Signal sig;
				bool is_signal = true;

				{
					Variant result = *argobj;

					if (argobj->get_type() == Variant::OBJECT) {
						bool was_freed = false;
						Object *obj = argobj->get_validated_object_with_check(was_freed);

						if (was_freed) {
							err_text = "Trying to await on a freed object.";
							OPCODE_BREAK;
						}

						// Is this even possible to be null at this point?
						if (obj) {
							if (obj->is_class_ptr(GDScriptFunctionState::get_class_ptr_static())) {
								result = Signal(obj, "completed");
							}
						}
					}

					if (result.get_type() != Variant::SIGNAL) {
						// Not async, return immediately using the target from OPCODE_AWAIT_RESUME.
						GET_VARIANT_PTR(target, 2);
						*target = result;
						ip += 4; // Skip OPCODE_AWAIT_RESUME and its data.
						is_signal = false;
					} else {
						sig = result;
					}
				}

				if (is_signal) {
					Ref<GDScriptFunctionState> gdfs = memnew(GDScriptFunctionState);
					gdfs->function = this;

					gdfs->state.stack.resize(alloca_size);

					// First 3 stack addresses are special, so we just skip them here.
					for (int i = 3; i < _stack_size; i++) {
						memnew_placement(&gdfs->state.stack.write[sizeof(Variant) * i], Variant(stack[i]));
					}
					gdfs->state.stack_size = _stack_size;
					gdfs->state.alloca_size = alloca_size;
					gdfs->state.ip = ip + 2;
					gdfs->state.line = line;
					gdfs->state.script = _script;
					{
						MutexLock lock(GDScriptLanguage::get_singleton()->mutex);
						_script->pending_func_states.add(&gdfs->scripts_list);
						if (p_instance) {
							gdfs->state.instance = p_instance;
							p_instance->pending_func_states.add(&gdfs->instances_list);
						} else {
							gdfs->state.instance = nullptr;
						}
					}
#ifdef DEBUG_ENABLED
					gdfs->state.function_name = name;
					gdfs->state.script_path = _script->get_script_path();
#endif
					gdfs->state.defarg = defarg;
					gdfs->function = this;

					retvalue = gdfs;

					Error err = sig.connect(Callable(gdfs.ptr(), "_signal_callback").bind(retvalue), Object::CONNECT_ONE_SHOT);
					if (err != OK) {
						err_text = "Error connecting to signal: " + sig.get_name() + " during await.";
						OPCODE_BREAK;
					}

#ifdef DEBUG_ENABLED
					exit_ok = true;
					awaited = true;
#endif
					OPCODE_BREAK;
				}
			}
			DISPATCH_OPCODE; // Needed for synchronous calls (when result is immediately available).

			OPCODE(OPCODE_AWAIT_RESUME) {
				CHECK_SPACE(2);
#ifdef DEBUG_ENABLED
				if (!p_state) {
					err_text = ("Invalid Resume (bug?)");
					OPCODE_BREAK;
				}
#endif
				GET_VARIANT_PTR(result, 0);
				*result = p_state->result;
				ip += 2;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CREATE_LAMBDA) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(2 + instr_arg_count);

				ip += instr_arg_count;

				int captures_count = _code_ptr[ip + 1];
				GD_ERR_BREAK(captures_count < 0);

				int lambda_index = _code_ptr[ip + 2];
				GD_ERR_BREAK(lambda_index < 0 || lambda_index >= _lambdas_count);
				GDScriptFunction *lambda = _lambdas_ptr[lambda_index];

				Vector<Variant> captures;
				captures.resize(captures_count);
				for (int i = 0; i < captures_count; i++) {
					GET_INSTRUCTION_ARG(arg, i);
					captures.write[i] = *arg;
				}

				GDScriptLambdaCallable *callable = memnew(GDScriptLambdaCallable(Ref<GDScript>(script), lambda, captures));

				GET_INSTRUCTION_ARG(result, captures_count);
				*result = Callable(callable);

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_CREATE_SELF_LAMBDA) {
				LOAD_INSTRUCTION_ARGS
				CHECK_SPACE(2 + instr_arg_count);

				GD_ERR_BREAK(p_instance == nullptr);

				ip += instr_arg_count;

				int captures_count = _code_ptr[ip + 1];
				GD_ERR_BREAK(captures_count < 0);

				int lambda_index = _code_ptr[ip + 2];
				GD_ERR_BREAK(lambda_index < 0 || lambda_index >= _lambdas_count);
				GDScriptFunction *lambda = _lambdas_ptr[lambda_index];

				Vector<Variant> captures;
				captures.resize(captures_count);
				for (int i = 0; i < captures_count; i++) {
					GET_INSTRUCTION_ARG(arg, i);
					captures.write[i] = *arg;
				}

				GDScriptLambdaSelfCallable *callable;
				if (Object::cast_to<RefCounted>(p_instance->owner)) {
					callable = memnew(GDScriptLambdaSelfCallable(Ref<RefCounted>(Object::cast_to<RefCounted>(p_instance->owner)), lambda, captures));
				} else {
					callable = memnew(GDScriptLambdaSelfCallable(p_instance->owner, lambda, captures));
				}

				GET_INSTRUCTION_ARG(result, captures_count);
				*result = Callable(callable);

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_JUMP) {
				CHECK_SPACE(2);
				int to = _code_ptr[ip + 1];

				GD_ERR_BREAK(to < 0 || to > _code_size);
				ip = to;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_JUMP_IF) {
				CHECK_SPACE(3);

				GET_VARIANT_PTR(test, 0);

				bool result = test->booleanize();

				if (result) {
					int to = _code_ptr[ip + 2];
					GD_ERR_BREAK(to < 0 || to > _code_size);
					ip = to;
				} else {
					ip += 3;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_JUMP_IF_NOT) {
				CHECK_SPACE(3);

				GET_VARIANT_PTR(test, 0);

				bool result = test->booleanize();

				if (!result) {
					int to = _code_ptr[ip + 2];
					GD_ERR_BREAK(to < 0 || to > _code_size);
					ip = to;
				} else {
					ip += 3;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_JUMP_TO_DEF_ARGUMENT) {
				CHECK_SPACE(2);
				ip = _default_arg_ptr[defarg];
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_JUMP_IF_SHARED) {
				CHECK_SPACE(3);

				GET_VARIANT_PTR(val, 0);

				if (val->is_shared()) {
					int to = _code_ptr[ip + 2];
					GD_ERR_BREAK(to < 0 || to > _code_size);
					ip = to;
				} else {
					ip += 3;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_RETURN) {
				CHECK_SPACE(2);
				GET_VARIANT_PTR(r, 0);
				retvalue = *r;
#ifdef DEBUG_ENABLED
				exit_ok = true;
#endif
				OPCODE_BREAK;
			}

			OPCODE(OPCODE_RETURN_TYPED_BUILTIN) {
				CHECK_SPACE(3);
				GET_VARIANT_PTR(r, 0);

				Variant::Type ret_type = (Variant::Type)_code_ptr[ip + 2];
				GD_ERR_BREAK(ret_type < 0 || ret_type >= Variant::VARIANT_MAX);

				if (r->get_type() != ret_type) {
					if (Variant::can_convert_strict(r->get_type(), ret_type)) {
						Callable::CallError ce;
						Variant::construct(ret_type, retvalue, const_cast<const Variant **>(&r), 1, ce);
					} else {
#ifdef DEBUG_ENABLED
						err_text = vformat(R"(Trying to return value of type "%s" from a function whose return type is "%s".)",
								Variant::get_type_name(r->get_type()), Variant::get_type_name(ret_type));
#endif // DEBUG_ENABLED

						// Construct a base type anyway so type constraints are met.
						Callable::CallError ce;
						Variant::construct(ret_type, retvalue, nullptr, 0, ce);
						OPCODE_BREAK;
					}
				} else {
					retvalue = *r;
				}
#ifdef DEBUG_ENABLED
				exit_ok = true;
#endif // DEBUG_ENABLED
				OPCODE_BREAK;
			}

			OPCODE(OPCODE_RETURN_TYPED_ARRAY) {
				CHECK_SPACE(5);
				GET_VARIANT_PTR(r, 0);

				GET_VARIANT_PTR(script_type, 1);
				Variant::Type builtin_type = (Variant::Type)_code_ptr[ip + 3];
				int native_type_idx = _code_ptr[ip + 4];
				GD_ERR_BREAK(native_type_idx < 0 || native_type_idx >= _global_names_count);
				const StringName native_type = _global_names_ptr[native_type_idx];

				if (r->get_type() != Variant::ARRAY) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to return value of type "%s" from a function whose return type is "Array[%s]".)",
							Variant::get_type_name(r->get_type()), Variant::get_type_name(builtin_type));
#endif
					OPCODE_BREAK;
				}

				Array *array = VariantInternal::get_array(r);

				if (array->get_typed_builtin() != ((uint32_t)builtin_type) || array->get_typed_class_name() != native_type || array->get_typed_script() != *script_type) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to return an array of type "%s" where expected return type is "Array[%s]".)",
							_get_var_type(r), _get_element_type(builtin_type, native_type, *script_type));
#endif // DEBUG_ENABLED
					OPCODE_BREAK;
				}

				retvalue = *array;

#ifdef DEBUG_ENABLED
				exit_ok = true;
#endif // DEBUG_ENABLED
				OPCODE_BREAK;
			}

			OPCODE(OPCODE_RETURN_TYPED_DICTIONARY) {
				CHECK_SPACE(8);
				GET_VARIANT_PTR(r, 0);

				GET_VARIANT_PTR(key_script_type, 1);
				Variant::Type key_builtin_type = (Variant::Type)_code_ptr[ip + 4];
				int key_native_type_idx = _code_ptr[ip + 5];
				GD_ERR_BREAK(key_native_type_idx < 0 || key_native_type_idx >= _global_names_count);
				const StringName key_native_type = _global_names_ptr[key_native_type_idx];

				GET_VARIANT_PTR(value_script_type, 2);
				Variant::Type value_builtin_type = (Variant::Type)_code_ptr[ip + 6];
				int value_native_type_idx = _code_ptr[ip + 7];
				GD_ERR_BREAK(value_native_type_idx < 0 || value_native_type_idx >= _global_names_count);
				const StringName value_native_type = _global_names_ptr[value_native_type_idx];

				if (r->get_type() != Variant::DICTIONARY) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to return a value of type "%s" where expected return type is "Dictionary[%s, %s]".)",
							_get_var_type(r), _get_element_type(key_builtin_type, key_native_type, *key_script_type),
							_get_element_type(value_builtin_type, value_native_type, *value_script_type));
#endif // DEBUG_ENABLED
					OPCODE_BREAK;
				}

				Dictionary *dictionary = VariantInternal::get_dictionary(r);

				if (dictionary->get_typed_key_builtin() != ((uint32_t)key_builtin_type) || dictionary->get_typed_key_class_name() != key_native_type || dictionary->get_typed_key_script() != *key_script_type ||
						dictionary->get_typed_value_builtin() != ((uint32_t)value_builtin_type) || dictionary->get_typed_value_class_name() != value_native_type || dictionary->get_typed_value_script() != *value_script_type) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to return a dictionary of type "%s" where expected return type is "Dictionary[%s, %s]".)",
							_get_var_type(r), _get_element_type(key_builtin_type, key_native_type, *key_script_type),
							_get_element_type(value_builtin_type, value_native_type, *value_script_type));
#endif // DEBUG_ENABLED
					OPCODE_BREAK;
				}

				retvalue = *dictionary;

#ifdef DEBUG_ENABLED
				exit_ok = true;
#endif // DEBUG_ENABLED
				OPCODE_BREAK;
			}

			OPCODE(OPCODE_RETURN_TYPED_NATIVE) {
				CHECK_SPACE(3);
				GET_VARIANT_PTR(r, 0);

				GET_VARIANT_PTR(type, 1);
				GDScriptNativeClass *nc = Object::cast_to<GDScriptNativeClass>(type->operator Object *());
				GD_ERR_BREAK(!nc);

				if (r->get_type() != Variant::OBJECT && r->get_type() != Variant::NIL) {
					err_text = vformat(R"(Trying to return value of type "%s" from a function whose return type is "%s".)",
							Variant::get_type_name(r->get_type()), nc->get_name());
					OPCODE_BREAK;
				}

#ifdef DEBUG_ENABLED
				bool freed = false;
				Object *ret_obj = r->get_validated_object_with_check(freed);

				if (freed) {
					err_text = "Trying to return a previously freed instance.";
					OPCODE_BREAK;
				}
#else
				Object *ret_obj = r->operator Object *();
#endif // DEBUG_ENABLED
				if (ret_obj && !ClassDB::is_parent_class(ret_obj->get_class_name(), nc->get_name())) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to return value of type "%s" from a function whose return type is "%s".)",
							ret_obj->get_class_name(), nc->get_name());
#endif // DEBUG_ENABLED
					OPCODE_BREAK;
				}
				retvalue = *r;

#ifdef DEBUG_ENABLED
				exit_ok = true;
#endif // DEBUG_ENABLED
				OPCODE_BREAK;
			}

			OPCODE(OPCODE_RETURN_TYPED_SCRIPT) {
				CHECK_SPACE(3);
				GET_VARIANT_PTR(r, 0);

				GET_VARIANT_PTR(type, 1);
				Script *base_type = Object::cast_to<Script>(type->operator Object *());
				GD_ERR_BREAK(!base_type);

				if (r->get_type() != Variant::OBJECT && r->get_type() != Variant::NIL) {
#ifdef DEBUG_ENABLED
					err_text = vformat(R"(Trying to return value of type "%s" from a function whose return type is "%s".)",
							Variant::get_type_name(r->get_type()), GDScript::debug_get_script_name(Ref<Script>(base_type)));
#endif // DEBUG_ENABLED
					OPCODE_BREAK;
				}

#ifdef DEBUG_ENABLED
				bool freed = false;
				Object *ret_obj = r->get_validated_object_with_check(freed);

				if (freed) {
					err_text = "Trying to return a previously freed instance.";
					OPCODE_BREAK;
				}
#else
				Object *ret_obj = r->operator Object *();
#endif // DEBUG_ENABLED

				if (ret_obj) {
					ScriptInstance *ret_inst = ret_obj->get_script_instance();
					if (!ret_inst) {
#ifdef DEBUG_ENABLED
						err_text = vformat(R"(Trying to return value of type "%s" from a function whose return type is "%s".)",
								ret_obj->get_class_name(), GDScript::debug_get_script_name(Ref<GDScript>(base_type)));
#endif // DEBUG_ENABLED
						OPCODE_BREAK;
					}

					Script *ret_type = ret_obj->get_script_instance()->get_script().ptr();
					bool valid = false;

					while (ret_type) {
						if (ret_type == base_type) {
							valid = true;
							break;
						}
						ret_type = ret_type->get_base_script().ptr();
					}

					if (!valid) {
#ifdef DEBUG_ENABLED
						err_text = vformat(R"(Trying to return value of type "%s" from a function whose return type is "%s".)",
								GDScript::debug_get_script_name(ret_obj->get_script_instance()->get_script()), GDScript::debug_get_script_name(Ref<GDScript>(base_type)));
#endif // DEBUG_ENABLED
						OPCODE_BREAK;
					}
				}
				retvalue = *r;

#ifdef DEBUG_ENABLED
				exit_ok = true;
#endif // DEBUG_ENABLED
				OPCODE_BREAK;
			}

			OPCODE(OPCODE_ITERATE_BEGIN) {
				CHECK_SPACE(8); // Space for this and a regular iterate.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				*counter = Variant();

				bool valid;
				if (!container->iter_init(*counter, valid)) {
#ifdef DEBUG_ENABLED
					if (!valid) {
						err_text = "Unable to iterate on object of type '" + Variant::get_type_name(container->get_type()) + "'.";
						OPCODE_BREAK;
					}
#endif
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);

					*iterator = container->iter_get(*counter, valid);
#ifdef DEBUG_ENABLED
					if (!valid) {
						err_text = "Unable to obtain iterator object of type '" + Variant::get_type_name(container->get_type()) + "'.";
						OPCODE_BREAK;
					}
#endif
					ip += 5; // Skip regular iterate which is always next.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_BEGIN_INT) {
				CHECK_SPACE(8); // Check space for iterate instruction too.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				int64_t size = *VariantInternal::get_int(container);

				VariantInternal::initialize(counter, Variant::INT);
				*VariantInternal::get_int(counter) = 0;

				if (size > 0) {
					GET_VARIANT_PTR(iterator, 2);
					VariantInternal::initialize(iterator, Variant::INT);
					*VariantInternal::get_int(iterator) = 0;

					// Skip regular iterate.
					ip += 5;
				} else {
					// Jump to end of loop.
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_BEGIN_FLOAT) {
				CHECK_SPACE(8); // Check space for iterate instruction too.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				double size = *VariantInternal::get_float(container);

				VariantInternal::initialize(counter, Variant::FLOAT);
				*VariantInternal::get_float(counter) = 0.0;

				if (size > 0) {
					GET_VARIANT_PTR(iterator, 2);
					VariantInternal::initialize(iterator, Variant::FLOAT);
					*VariantInternal::get_float(iterator) = 0;

					// Skip regular iterate.
					ip += 5;
				} else {
					// Jump to end of loop.
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_BEGIN_VECTOR2) {
				CHECK_SPACE(8); // Check space for iterate instruction too.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				Vector2 *bounds = VariantInternal::get_vector2(container);

				VariantInternal::initialize(counter, Variant::FLOAT);
				*VariantInternal::get_float(counter) = bounds->x;

				if (bounds->x < bounds->y) {
					GET_VARIANT_PTR(iterator, 2);
					VariantInternal::initialize(iterator, Variant::FLOAT);
					*VariantInternal::get_float(iterator) = bounds->x;

					// Skip regular iterate.
					ip += 5;
				} else {
					// Jump to end of loop.
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_BEGIN_VECTOR2I) {
				CHECK_SPACE(8); // Check space for iterate instruction too.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				Vector2i *bounds = VariantInternal::get_vector2i(container);

				VariantInternal::initialize(counter, Variant::FLOAT);
				*VariantInternal::get_int(counter) = bounds->x;

				if (bounds->x < bounds->y) {
					GET_VARIANT_PTR(iterator, 2);
					VariantInternal::initialize(iterator, Variant::INT);
					*VariantInternal::get_int(iterator) = bounds->x;

					// Skip regular iterate.
					ip += 5;
				} else {
					// Jump to end of loop.
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_BEGIN_VECTOR3) {
				CHECK_SPACE(8); // Check space for iterate instruction too.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				Vector3 *bounds = VariantInternal::get_vector3(container);
				double from = bounds->x;
				double to = bounds->y;
				double step = bounds->z;

				VariantInternal::initialize(counter, Variant::FLOAT);
				*VariantInternal::get_float(counter) = from;

				bool do_continue = from == to ? false : (from < to ? step > 0 : step < 0);

				if (do_continue) {
					GET_VARIANT_PTR(iterator, 2);
					VariantInternal::initialize(iterator, Variant::FLOAT);
					*VariantInternal::get_float(iterator) = from;

					// Skip regular iterate.
					ip += 5;
				} else {
					// Jump to end of loop.
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_BEGIN_VECTOR3I) {
				CHECK_SPACE(8); // Check space for iterate instruction too.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				Vector3i *bounds = VariantInternal::get_vector3i(container);
				int64_t from = bounds->x;
				int64_t to = bounds->y;
				int64_t step = bounds->z;

				VariantInternal::initialize(counter, Variant::INT);
				*VariantInternal::get_int(counter) = from;

				bool do_continue = from == to ? false : (from < to ? step > 0 : step < 0);

				if (do_continue) {
					GET_VARIANT_PTR(iterator, 2);
					VariantInternal::initialize(iterator, Variant::INT);
					*VariantInternal::get_int(iterator) = from;

					// Skip regular iterate.
					ip += 5;
				} else {
					// Jump to end of loop.
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_BEGIN_STRING) {
				CHECK_SPACE(8); // Check space for iterate instruction too.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				String *str = VariantInternal::get_string(container);

				VariantInternal::initialize(counter, Variant::INT);
				*VariantInternal::get_int(counter) = 0;

				if (!str->is_empty()) {
					GET_VARIANT_PTR(iterator, 2);
					VariantInternal::initialize(iterator, Variant::STRING);
					*VariantInternal::get_string(iterator) = str->substr(0, 1);

					// Skip regular iterate.
					ip += 5;
				} else {
					// Jump to end of loop.
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_BEGIN_DICTIONARY) {
				CHECK_SPACE(8); // Check space for iterate instruction too.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				Dictionary *dict = VariantInternal::get_dictionary(container);
				const Variant *next = dict->next(nullptr);

				if (!dict->is_empty()) {
					GET_VARIANT_PTR(iterator, 2);
					*counter = *next;
					*iterator = *next;

					// Skip regular iterate.
					ip += 5;
				} else {
					// Jump to end of loop.
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_BEGIN_ARRAY) {
				CHECK_SPACE(8); // Check space for iterate instruction too.

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				Array *array = VariantInternal::get_array(container);

				VariantInternal::initialize(counter, Variant::INT);
				*VariantInternal::get_int(counter) = 0;

				if (!array->is_empty()) {
					GET_VARIANT_PTR(iterator, 2);
					*iterator = array->get(0);

					// Skip regular iterate.
					ip += 5;
				} else {
					// Jump to end of loop.
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				}
			}
			DISPATCH_OPCODE;

#define OPCODE_ITERATE_BEGIN_PACKED_ARRAY(m_var_type, m_elem_type, m_get_func, m_var_ret_type, m_ret_type, m_ret_get_func) \
	OPCODE(OPCODE_ITERATE_BEGIN_PACKED_##m_var_type##_ARRAY) {                                                             \
		CHECK_SPACE(8);                                                                                                    \
		GET_VARIANT_PTR(counter, 0);                                                                                       \
		GET_VARIANT_PTR(container, 1);                                                                                     \
		Vector<m_elem_type> *array = VariantInternal::m_get_func(container);                                               \
		VariantInternal::initialize(counter, Variant::INT);                                                                \
		*VariantInternal::get_int(counter) = 0;                                                                            \
		if (!array->is_empty()) {                                                                                          \
			GET_VARIANT_PTR(iterator, 2);                                                                                  \
			VariantInternal::initialize(iterator, Variant::m_var_ret_type);                                                \
			m_ret_type *it = VariantInternal::m_ret_get_func(iterator);                                                    \
			*it = array->get(0);                                                                                           \
			ip += 5;                                                                                                       \
		} else {                                                                                                           \
			int jumpto = _code_ptr[ip + 4];                                                                                \
			GD_ERR_BREAK(jumpto<0 || jumpto> _code_size);                                                                  \
			ip = jumpto;                                                                                                   \
		}                                                                                                                  \
	}                                                                                                                      \
	DISPATCH_OPCODE

			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(BYTE, uint8_t, get_byte_array, INT, int64_t, get_int);
			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(INT32, int32_t, get_int32_array, INT, int64_t, get_int);
			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(INT64, int64_t, get_int64_array, INT, int64_t, get_int);
			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(FLOAT32, float, get_float32_array, FLOAT, double, get_float);
			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(FLOAT64, double, get_float64_array, FLOAT, double, get_float);
			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(STRING, String, get_string_array, STRING, String, get_string);
			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(VECTOR2, Vector2, get_vector2_array, VECTOR2, Vector2, get_vector2);
			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(VECTOR3, Vector3, get_vector3_array, VECTOR3, Vector3, get_vector3);
			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(COLOR, Color, get_color_array, COLOR, Color, get_color);
			OPCODE_ITERATE_BEGIN_PACKED_ARRAY(VECTOR4, Vector4, get_vector4_array, VECTOR4, Vector4, get_vector4);

			OPCODE(OPCODE_ITERATE_BEGIN_OBJECT) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

#ifdef DEBUG_ENABLED
				bool freed = false;
				Object *obj = container->get_validated_object_with_check(freed);
				if (freed) {
					err_text = "Trying to iterate on a previously freed object.";
					OPCODE_BREAK;
				} else if (!obj) {
					err_text = "Trying to iterate on a null value.";
					OPCODE_BREAK;
				}
#else
				Object *obj = *VariantInternal::get_object(container);
#endif

				*counter = Variant();

				Array ref;
				ref.push_back(*counter);
				Variant vref;
				VariantInternal::initialize(&vref, Variant::ARRAY);
				*VariantInternal::get_array(&vref) = ref;

				const Variant *args[] = { &vref };

				Callable::CallError ce;
				Variant has_next = obj->callp(CoreStringName(_iter_init), args, 1, ce);

#ifdef DEBUG_ENABLED
				if (ref.size() != 1 || ce.error != Callable::CallError::CALL_OK) {
					err_text = vformat(R"(There was an error calling "_iter_next" on iterator object of type %s.)", *container);
					OPCODE_BREAK;
				}
#endif
				if (!has_next.booleanize()) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					*counter = ref[0];

					GET_VARIANT_PTR(iterator, 2);
					*iterator = obj->callp(CoreStringName(_iter_get), (const Variant **)&counter, 1, ce);
#ifdef DEBUG_ENABLED
					if (ce.error != Callable::CallError::CALL_OK) {
						err_text = vformat(R"(There was an error calling "_iter_get" on iterator object of type %s.)", *container);
						OPCODE_BREAK;
					}
#endif

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				bool valid;
				if (!container->iter_next(*counter, valid)) {
#ifdef DEBUG_ENABLED
					if (!valid) {
						err_text = "Unable to iterate on object of type '" + Variant::get_type_name(container->get_type()) + "' (type changed since first iteration?).";
						OPCODE_BREAK;
					}
#endif
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);

					*iterator = container->iter_get(*counter, valid);
#ifdef DEBUG_ENABLED
					if (!valid) {
						err_text = "Unable to obtain iterator object of type '" + Variant::get_type_name(container->get_type()) + "' (but was obtained on first iteration?).";
						OPCODE_BREAK;
					}
#endif
					ip += 5; //loop again
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_INT) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				int64_t size = *VariantInternal::get_int(container);
				int64_t *count = VariantInternal::get_int(counter);

				(*count)++;

				if (*count >= size) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);
					*VariantInternal::get_int(iterator) = *count;

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_FLOAT) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				double size = *VariantInternal::get_float(container);
				double *count = VariantInternal::get_float(counter);

				(*count)++;

				if (*count >= size) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);
					*VariantInternal::get_float(iterator) = *count;

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_VECTOR2) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				const Vector2 *bounds = VariantInternal::get_vector2((const Variant *)container);
				double *count = VariantInternal::get_float(counter);

				(*count)++;

				if (*count >= bounds->y) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);
					*VariantInternal::get_float(iterator) = *count;

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_VECTOR2I) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				const Vector2i *bounds = VariantInternal::get_vector2i((const Variant *)container);
				int64_t *count = VariantInternal::get_int(counter);

				(*count)++;

				if (*count >= bounds->y) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);
					*VariantInternal::get_int(iterator) = *count;

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_VECTOR3) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				const Vector3 *bounds = VariantInternal::get_vector3((const Variant *)container);
				double *count = VariantInternal::get_float(counter);

				*count += bounds->z;

				if ((bounds->z < 0 && *count <= bounds->y) || (bounds->z > 0 && *count >= bounds->y)) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);
					*VariantInternal::get_float(iterator) = *count;

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_VECTOR3I) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				const Vector3i *bounds = VariantInternal::get_vector3i((const Variant *)container);
				int64_t *count = VariantInternal::get_int(counter);

				*count += bounds->z;

				if ((bounds->z < 0 && *count <= bounds->y) || (bounds->z > 0 && *count >= bounds->y)) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);
					*VariantInternal::get_int(iterator) = *count;

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_STRING) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				const String *str = VariantInternal::get_string((const Variant *)container);
				int64_t *idx = VariantInternal::get_int(counter);
				(*idx)++;

				if (*idx >= str->length()) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);
					*VariantInternal::get_string(iterator) = str->substr(*idx, 1);

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_DICTIONARY) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				const Dictionary *dict = VariantInternal::get_dictionary((const Variant *)container);
				const Variant *next = dict->next(counter);

				if (!next) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);
					*counter = *next;
					*iterator = *next;

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_ITERATE_ARRAY) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

				const Array *array = VariantInternal::get_array((const Variant *)container);
				int64_t *idx = VariantInternal::get_int(counter);
				(*idx)++;

				if (*idx >= array->size()) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					GET_VARIANT_PTR(iterator, 2);
					*iterator = array->get(*idx);

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

#define OPCODE_ITERATE_PACKED_ARRAY(m_var_type, m_elem_type, m_get_func, m_ret_get_func)            \
	OPCODE(OPCODE_ITERATE_PACKED_##m_var_type##_ARRAY) {                                            \
		CHECK_SPACE(4);                                                                             \
		GET_VARIANT_PTR(counter, 0);                                                                \
		GET_VARIANT_PTR(container, 1);                                                              \
		const Vector<m_elem_type> *array = VariantInternal::m_get_func((const Variant *)container); \
		int64_t *idx = VariantInternal::get_int(counter);                                           \
		(*idx)++;                                                                                   \
		if (*idx >= array->size()) {                                                                \
			int jumpto = _code_ptr[ip + 4];                                                         \
			GD_ERR_BREAK(jumpto<0 || jumpto> _code_size);                                           \
			ip = jumpto;                                                                            \
		} else {                                                                                    \
			GET_VARIANT_PTR(iterator, 2);                                                           \
			*VariantInternal::m_ret_get_func(iterator) = array->get(*idx);                          \
			ip += 5;                                                                                \
		}                                                                                           \
	}                                                                                               \
	DISPATCH_OPCODE

			OPCODE_ITERATE_PACKED_ARRAY(BYTE, uint8_t, get_byte_array, get_int);
			OPCODE_ITERATE_PACKED_ARRAY(INT32, int32_t, get_int32_array, get_int);
			OPCODE_ITERATE_PACKED_ARRAY(INT64, int64_t, get_int64_array, get_int);
			OPCODE_ITERATE_PACKED_ARRAY(FLOAT32, float, get_float32_array, get_float);
			OPCODE_ITERATE_PACKED_ARRAY(FLOAT64, double, get_float64_array, get_float);
			OPCODE_ITERATE_PACKED_ARRAY(STRING, String, get_string_array, get_string);
			OPCODE_ITERATE_PACKED_ARRAY(VECTOR2, Vector2, get_vector2_array, get_vector2);
			OPCODE_ITERATE_PACKED_ARRAY(VECTOR3, Vector3, get_vector3_array, get_vector3);
			OPCODE_ITERATE_PACKED_ARRAY(COLOR, Color, get_color_array, get_color);
			OPCODE_ITERATE_PACKED_ARRAY(VECTOR4, Vector4, get_vector4_array, get_vector4);

			OPCODE(OPCODE_ITERATE_OBJECT) {
				CHECK_SPACE(4);

				GET_VARIANT_PTR(counter, 0);
				GET_VARIANT_PTR(container, 1);

#ifdef DEBUG_ENABLED
				bool freed = false;
				Object *obj = container->get_validated_object_with_check(freed);
				if (freed) {
					err_text = "Trying to iterate on a previously freed object.";
					OPCODE_BREAK;
				} else if (!obj) {
					err_text = "Trying to iterate on a null value.";
					OPCODE_BREAK;
				}
#else
				Object *obj = *VariantInternal::get_object(container);
#endif

				Array ref;
				ref.push_back(*counter);
				Variant vref;
				VariantInternal::initialize(&vref, Variant::ARRAY);
				*VariantInternal::get_array(&vref) = ref;

				const Variant *args[] = { &vref };

				Callable::CallError ce;
				Variant has_next = obj->callp(CoreStringName(_iter_next), args, 1, ce);

#ifdef DEBUG_ENABLED
				if (ref.size() != 1 || ce.error != Callable::CallError::CALL_OK) {
					err_text = vformat(R"(There was an error calling "_iter_next" on iterator object of type %s.)", *container);
					OPCODE_BREAK;
				}
#endif
				if (!has_next.booleanize()) {
					int jumpto = _code_ptr[ip + 4];
					GD_ERR_BREAK(jumpto < 0 || jumpto > _code_size);
					ip = jumpto;
				} else {
					*counter = ref[0];

					GET_VARIANT_PTR(iterator, 2);
					*iterator = obj->callp(CoreStringName(_iter_get), (const Variant **)&counter, 1, ce);
#ifdef DEBUG_ENABLED
					if (ce.error != Callable::CallError::CALL_OK) {
						err_text = vformat(R"(There was an error calling "_iter_get" on iterator object of type %s.)", *container);
						OPCODE_BREAK;
					}
#endif

					ip += 5; // Loop again.
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_STORE_GLOBAL) {
				CHECK_SPACE(3);
				int global_idx = _code_ptr[ip + 2];
				GD_ERR_BREAK(global_idx < 0 || global_idx >= GDScriptLanguage::get_singleton()->get_global_array_size());

				GET_VARIANT_PTR(dst, 0);
				*dst = GDScriptLanguage::get_singleton()->get_global_array()[global_idx];

				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_STORE_NAMED_GLOBAL) {
				CHECK_SPACE(3);
				int globalname_idx = _code_ptr[ip + 2];
				GD_ERR_BREAK(globalname_idx < 0 || globalname_idx >= _global_names_count);
				const StringName *globalname = &_global_names_ptr[globalname_idx];
				GD_ERR_BREAK(!GDScriptLanguage::get_singleton()->get_named_globals_map().has(*globalname));

				GET_VARIANT_PTR(dst, 0);
				*dst = GDScriptLanguage::get_singleton()->get_named_globals_map()[*globalname];

				ip += 3;
			}
			DISPATCH_OPCODE;

#define OPCODE_TYPE_ADJUST(m_v_type, m_c_type)    \
	OPCODE(OPCODE_TYPE_ADJUST_##m_v_type) {       \
		CHECK_SPACE(2);                           \
		GET_VARIANT_PTR(arg, 0);                  \
		VariantTypeAdjust<m_c_type>::adjust(arg); \
		ip += 2;                                  \
	}                                             \
	DISPATCH_OPCODE

			OPCODE_TYPE_ADJUST(BOOL, bool);
			OPCODE_TYPE_ADJUST(INT, int64_t);
			OPCODE_TYPE_ADJUST(FLOAT, double);
			OPCODE_TYPE_ADJUST(STRING, String);
			OPCODE_TYPE_ADJUST(VECTOR2, Vector2);
			OPCODE_TYPE_ADJUST(VECTOR2I, Vector2i);
			OPCODE_TYPE_ADJUST(RECT2, Rect2);
			OPCODE_TYPE_ADJUST(RECT2I, Rect2i);
			OPCODE_TYPE_ADJUST(VECTOR3, Vector3);
			OPCODE_TYPE_ADJUST(VECTOR3I, Vector3i);
			OPCODE_TYPE_ADJUST(TRANSFORM2D, Transform2D);
			OPCODE_TYPE_ADJUST(VECTOR4, Vector4);
			OPCODE_TYPE_ADJUST(VECTOR4I, Vector4i);
			OPCODE_TYPE_ADJUST(PLANE, Plane);
			OPCODE_TYPE_ADJUST(QUATERNION, Quaternion);
			OPCODE_TYPE_ADJUST(AABB, AABB);
			OPCODE_TYPE_ADJUST(BASIS, Basis);
			OPCODE_TYPE_ADJUST(TRANSFORM3D, Transform3D);
			OPCODE_TYPE_ADJUST(PROJECTION, Projection);
			OPCODE_TYPE_ADJUST(COLOR, Color);
			OPCODE_TYPE_ADJUST(STRING_NAME, StringName);
			OPCODE_TYPE_ADJUST(NODE_PATH, NodePath);
			OPCODE_TYPE_ADJUST(RID, RID);
			OPCODE_TYPE_ADJUST(OBJECT, Object *);
			OPCODE_TYPE_ADJUST(CALLABLE, Callable);
			OPCODE_TYPE_ADJUST(SIGNAL, Signal);
			OPCODE_TYPE_ADJUST(DICTIONARY, Dictionary);
			OPCODE_TYPE_ADJUST(ARRAY, Array);
			OPCODE_TYPE_ADJUST(PACKED_BYTE_ARRAY, PackedByteArray);
			OPCODE_TYPE_ADJUST(PACKED_INT32_ARRAY, PackedInt32Array);
			OPCODE_TYPE_ADJUST(PACKED_INT64_ARRAY, PackedInt64Array);
			OPCODE_TYPE_ADJUST(PACKED_FLOAT32_ARRAY, PackedFloat32Array);
			OPCODE_TYPE_ADJUST(PACKED_FLOAT64_ARRAY, PackedFloat64Array);
			OPCODE_TYPE_ADJUST(PACKED_STRING_ARRAY, PackedStringArray);
			OPCODE_TYPE_ADJUST(PACKED_VECTOR2_ARRAY, PackedVector2Array);
			OPCODE_TYPE_ADJUST(PACKED_VECTOR3_ARRAY, PackedVector3Array);
			OPCODE_TYPE_ADJUST(PACKED_COLOR_ARRAY, PackedColorArray);
			OPCODE_TYPE_ADJUST(PACKED_VECTOR4_ARRAY, PackedVector4Array);

			OPCODE(OPCODE_ASSERT) {
				CHECK_SPACE(3);

#ifdef DEBUG_ENABLED
				GET_VARIANT_PTR(test, 0);
				bool result = test->booleanize();

				if (!result) {
					String message_str;
					if (_code_ptr[ip + 2] != 0) {
						GET_VARIANT_PTR(message, 1);
						Variant message_var = *message;
						if (message->get_type() != Variant::NIL) {
							message_str = message_var;
						}
					}
					if (message_str.is_empty()) {
						err_text = "Assertion failed.";
					} else {
						err_text = "Assertion failed: " + message_str;
					}
					OPCODE_BREAK;
				}

#endif
				ip += 3;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_BREAKPOINT) {
#ifdef DEBUG_ENABLED
				if (EngineDebugger::is_active()) {
					GDScriptLanguage::get_singleton()->debug_break("Breakpoint Statement", true);
				}
#endif
				ip += 1;
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_LINE) {
				CHECK_SPACE(2);

				line = _code_ptr[ip + 1];
				ip += 2;

				if (EngineDebugger::is_active()) {
					// line
					bool do_break = false;

					if (unlikely(EngineDebugger::get_script_debugger()->get_lines_left() > 0)) {
						if (EngineDebugger::get_script_debugger()->get_depth() <= 0) {
							EngineDebugger::get_script_debugger()->set_lines_left(EngineDebugger::get_script_debugger()->get_lines_left() - 1);
						}
						if (EngineDebugger::get_script_debugger()->get_lines_left() <= 0) {
							do_break = true;
						}
					}

					if (EngineDebugger::get_script_debugger()->is_breakpoint(line, source)) {
						do_break = true;
					}

					if (unlikely(do_break)) {
						GDScriptLanguage::get_singleton()->debug_break("Breakpoint", true);
					}

					EngineDebugger::get_singleton()->line_poll();
				}
			}
			DISPATCH_OPCODE;

			OPCODE(OPCODE_END) {
#ifdef DEBUG_ENABLED
				exit_ok = true;
#endif
				OPCODE_BREAK;
			}

#if 0 // Enable for debugging.
			default: {
				err_text = "Illegal opcode " + itos(_code_ptr[ip]) + " at address " + itos(ip);
				OPCODE_BREAK;
			}
#endif
		}

		OPCODES_END
#ifdef DEBUG_ENABLED
		if (exit_ok) {
			OPCODE_OUT;
		}
		//error
		// function, file, line, error, explanation
		String err_file;
		bool instance_valid_with_script = p_instance && ObjectDB::get_instance(p_instance->owner_id) != nullptr && p_instance->script->is_valid();
		if (instance_valid_with_script && !get_script()->path.is_empty()) {
			err_file = get_script()->path;
		} else if (script) {
			err_file = script->path;
		}
		if (err_file.is_empty()) {
			err_file = "<built-in>";
		}
		String err_func = name;
		if (instance_valid_with_script && p_instance->script->local_name != StringName()) {
			err_func = p_instance->script->local_name.operator String() + "." + err_func;
		}
		int err_line = line;
		if (err_text.is_empty()) {
			err_text = "Internal script error! Opcode: " + itos(last_opcode) + " (please report).";
		}

		if (!GDScriptLanguage::get_singleton()->debug_break(err_text, false)) {
			// debugger break did not happen

			_err_print_error(err_func.utf8().get_data(), err_file.utf8().get_data(), err_line, err_text.utf8().get_data(), false, ERR_HANDLER_SCRIPT);
		}

		// Get a default return type in case of failure
		retvalue = _get_default_variant_for_data_type(return_type);
#endif

		OPCODE_OUT;
	}

	OPCODES_OUT
#ifdef DEBUG_ENABLED
	if (GDScriptLanguage::get_singleton()->profiling) {
		uint64_t time_taken = OS::get_singleton()->get_ticks_usec() - function_start_time;
		profile.total_time.add(time_taken);
		profile.self_time.add(time_taken - function_call_time);
		profile.frame_total_time.add(time_taken);
		profile.frame_self_time.add(time_taken - function_call_time);
		if (Thread::get_caller_id() == Thread::get_main_id()) {
			GDScriptLanguage::get_singleton()->script_frame_time += time_taken - function_call_time;
		}
	}

	// Check if this is not the last time it was interrupted by `await` or if it's the first time executing.
	// If that is the case then we exit the function as normal. Otherwise we postpone it until the last `await` is completed.
	// This ensures the call stack can be properly shown when using `await`, showing what resumed the function.
	if (!p_state || awaited) {
		if (EngineDebugger::is_active()) {
			GDScriptLanguage::get_singleton()->exit_function();
		}
#endif

		// Free stack, except reserved addresses.
		for (int i = FIXED_ADDRESSES_MAX; i < _stack_size; i++) {
			stack[i].~Variant();
		}
#ifdef DEBUG_ENABLED
	}
#endif

	// Always free reserved addresses, since they are never copied.
	for (int i = 0; i < FIXED_ADDRESSES_MAX; i++) {
		stack[i].~Variant();
	}

	call_depth--;

	return retvalue;
}
