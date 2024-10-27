/**************************************************************************/
/*  gdscript_utility_functions.cpp                                        */
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

#include "gdscript_utility_functions.h"

#include "gdscript.h"

#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/oa_hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/typed_array.h"

#ifdef DEBUG_ENABLED

#define DEBUG_VALIDATE_ARG_COUNT(m_min_count, m_max_count)                  \
	if (unlikely(p_arg_count < m_min_count)) {                              \
		*r_ret = Variant();                                                 \
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;  \
		r_error.expected = m_min_count;                                     \
		return;                                                             \
	}                                                                       \
	if (unlikely(p_arg_count > m_max_count)) {                              \
		*r_ret = Variant();                                                 \
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS; \
		r_error.expected = m_max_count;                                     \
		return;                                                             \
	}

#define DEBUG_VALIDATE_ARG_TYPE(m_arg, m_type)                                       \
	if (unlikely(!Variant::can_convert_strict(p_args[m_arg]->get_type(), m_type))) { \
		*r_ret = Variant();                                                          \
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;            \
		r_error.argument = m_arg;                                                    \
		r_error.expected = m_type;                                                   \
		return;                                                                      \
	}

#define DEBUG_VALIDATE_ARG_CUSTOM(m_arg, m_type, m_cond, m_msg)           \
	if (unlikely(m_cond)) {                                               \
		*r_ret = m_msg;                                                   \
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT; \
		r_error.argument = m_arg;                                         \
		r_error.expected = m_type;                                        \
		return;                                                           \
	}

#else // !DEBUG_ENABLED

#define DEBUG_VALIDATE_ARG_COUNT(m_min_count, m_max_count)
#define DEBUG_VALIDATE_ARG_TYPE(m_arg, m_type)
#define DEBUG_VALIDATE_ARG_CUSTOM(m_arg, m_type, m_cond, m_msg)

#endif // DEBUG_ENABLED

#define VALIDATE_ARG_CUSTOM(m_arg, m_type, m_cond, m_msg)                 \
	if (unlikely(m_cond)) {                                               \
		*r_ret = m_msg;                                                   \
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT; \
		r_error.argument = m_arg;                                         \
		r_error.expected = m_type;                                        \
		return;                                                           \
	}

#define GDFUNC_FAIL_COND_MSG(m_cond, m_msg)                             \
	if (unlikely(m_cond)) {                                             \
		*r_ret = m_msg;                                                 \
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD; \
		return;                                                         \
	}

struct GDScriptUtilityFunctionsDefinitions {
#ifndef DISABLE_DEPRECATED
	static inline void convert(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(2, 2);
		DEBUG_VALIDATE_ARG_TYPE(1, Variant::INT);

		int type = *p_args[1];
		DEBUG_VALIDATE_ARG_CUSTOM(1, Variant::INT, type < 0 || type >= Variant::VARIANT_MAX,
				RTR("Invalid type argument to convert(), use TYPE_* constants."));

		Variant::construct(Variant::Type(type), *r_ret, p_args, 1, r_error);
	}
#endif // DISABLE_DEPRECATED

	static inline void type_exists(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(1, 1);
		DEBUG_VALIDATE_ARG_TYPE(0, Variant::STRING_NAME);
		*r_ret = ClassDB::class_exists(*p_args[0]);
	}

	static inline void _char(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(1, 1);
		DEBUG_VALIDATE_ARG_TYPE(0, Variant::INT);
		char32_t result[2] = { *p_args[0], 0 };
		*r_ret = String(result);
	}

	static inline void range(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(1, 3);
		switch (p_arg_count) {
			case 1: {
				DEBUG_VALIDATE_ARG_TYPE(0, Variant::INT);

				int count = *p_args[0];

				Array arr;
				if (count <= 0) {
					*r_ret = arr;
					return;
				}

				Error err = arr.resize(count);
				GDFUNC_FAIL_COND_MSG(err != OK, RTR("Cannot resize array."));

				for (int i = 0; i < count; i++) {
					arr[i] = i;
				}

				*r_ret = arr;
			} break;
			case 2: {
				DEBUG_VALIDATE_ARG_TYPE(0, Variant::INT);
				DEBUG_VALIDATE_ARG_TYPE(1, Variant::INT);

				int from = *p_args[0];
				int to = *p_args[1];

				Array arr;
				if (from >= to) {
					*r_ret = arr;
					return;
				}

				Error err = arr.resize(to - from);
				GDFUNC_FAIL_COND_MSG(err != OK, RTR("Cannot resize array."));

				for (int i = from; i < to; i++) {
					arr[i - from] = i;
				}

				*r_ret = arr;
			} break;
			case 3: {
				DEBUG_VALIDATE_ARG_TYPE(0, Variant::INT);
				DEBUG_VALIDATE_ARG_TYPE(1, Variant::INT);
				DEBUG_VALIDATE_ARG_TYPE(2, Variant::INT);

				int from = *p_args[0];
				int to = *p_args[1];
				int incr = *p_args[2];

				VALIDATE_ARG_CUSTOM(2, Variant::INT, incr == 0, RTR("Step argument is zero!"));

				Array arr;
				if (from >= to && incr > 0) {
					*r_ret = arr;
					return;
				}
				if (from <= to && incr < 0) {
					*r_ret = arr;
					return;
				}

				// Calculate how many.
				int count = 0;
				if (incr > 0) {
					count = Math::division_round_up(to - from, incr);
				} else {
					count = Math::division_round_up(from - to, -incr);
				}

				Error err = arr.resize(count);
				GDFUNC_FAIL_COND_MSG(err != OK, RTR("Cannot resize array."));

				if (incr > 0) {
					int idx = 0;
					for (int i = from; i < to; i += incr) {
						arr[idx++] = i;
					}
				} else {
					int idx = 0;
					for (int i = from; i > to; i += incr) {
						arr[idx++] = i;
					}
				}

				*r_ret = arr;
			} break;
		}
	}

	static inline void load(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(1, 1);
		DEBUG_VALIDATE_ARG_TYPE(0, Variant::STRING);
		*r_ret = ResourceLoader::load(*p_args[0]);
	}

	static inline void inst_to_dict(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(1, 1);
		DEBUG_VALIDATE_ARG_TYPE(0, Variant::OBJECT);

		if (p_args[0]->get_type() == Variant::NIL) {
			*r_ret = Variant();
			return;
		}

		Object *obj = *p_args[0];
		if (!obj) {
			*r_ret = Variant();
			return;
		}

		VALIDATE_ARG_CUSTOM(0, Variant::OBJECT,
				!obj->get_script_instance() || obj->get_script_instance()->get_language() != GDScriptLanguage::get_singleton(),
				RTR("Not a script with an instance."));

		GDScriptInstance *inst = static_cast<GDScriptInstance *>(obj->get_script_instance());

		Ref<GDScript> base = inst->get_script();
		VALIDATE_ARG_CUSTOM(0, Variant::OBJECT, base.is_null(), RTR("Not based on a script."));

		GDScript *p = base.ptr();
		String path = p->get_script_path();
		Vector<StringName> sname;

		while (p->_owner) {
			sname.push_back(p->local_name);
			p = p->_owner;
		}
		sname.reverse();

		VALIDATE_ARG_CUSTOM(0, Variant::OBJECT, !path.is_resource_file(), RTR("Not based on a resource file."));

		NodePath cp(sname, Vector<StringName>(), false);

		Dictionary d;
		d["@subpath"] = cp;
		d["@path"] = path;

		for (const KeyValue<StringName, GDScript::MemberInfo> &E : base->member_indices) {
			if (!d.has(E.key)) {
				d[E.key] = inst->members[E.value.index];
			}
		}

		*r_ret = d;
	}

	static inline void dict_to_inst(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(1, 1);
		DEBUG_VALIDATE_ARG_TYPE(0, Variant::DICTIONARY);

		Dictionary d = *p_args[0];

		VALIDATE_ARG_CUSTOM(0, Variant::DICTIONARY, !d.has("@path"), RTR("Invalid instance dictionary format (missing @path)."));

		Ref<Script> scr = ResourceLoader::load(d["@path"]);
		VALIDATE_ARG_CUSTOM(0, Variant::DICTIONARY, !scr.is_valid(), RTR("Invalid instance dictionary format (can't load script at @path)."));

		Ref<GDScript> gdscr = scr;
		VALIDATE_ARG_CUSTOM(0, Variant::DICTIONARY, !gdscr.is_valid(), RTR("Invalid instance dictionary format (invalid script at @path)."));

		NodePath sub;
		if (d.has("@subpath")) {
			sub = d["@subpath"];
		}

		for (int i = 0; i < sub.get_name_count(); i++) {
			gdscr = gdscr->subclasses[sub.get_name(i)];
			VALIDATE_ARG_CUSTOM(0, Variant::DICTIONARY, !gdscr.is_valid(), RTR("Invalid instance dictionary (invalid subclasses)."));
		}

		*r_ret = gdscr->_new(nullptr, -1 /* skip initializer */, r_error);
		if (r_error.error != Callable::CallError::CALL_OK) {
			*r_ret = RTR("Cannot instantiate GDScript class.");
			return;
		}

		GDScriptInstance *inst = static_cast<GDScriptInstance *>(static_cast<Object *>(*r_ret)->get_script_instance());
		Ref<GDScript> gd_ref = inst->get_script();

		for (KeyValue<StringName, GDScript::MemberInfo> &E : gd_ref->member_indices) {
			if (d.has(E.key)) {
				inst->members.write[E.value.index] = d[E.key];
			}
		}
	}

	static inline void Color8(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(3, 4);
		DEBUG_VALIDATE_ARG_TYPE(0, Variant::INT);
		DEBUG_VALIDATE_ARG_TYPE(1, Variant::INT);
		DEBUG_VALIDATE_ARG_TYPE(2, Variant::INT);

		Color color((int64_t)*p_args[0] / 255.0f, (int64_t)*p_args[1] / 255.0f, (int64_t)*p_args[2] / 255.0f);

		if (p_arg_count == 4) {
			DEBUG_VALIDATE_ARG_TYPE(3, Variant::INT);
			color.a = (int64_t)*p_args[3] / 255.0f;
		}

		*r_ret = color;
	}

	static inline void print_debug(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		String s;
		for (int i = 0; i < p_arg_count; i++) {
			s += p_args[i]->operator String();
		}

		if (Thread::get_caller_id() == Thread::get_main_id()) {
			ScriptLanguage *script = GDScriptLanguage::get_singleton();
			if (script->debug_get_stack_level_count() > 0) {
				s += "\n   At: " + script->debug_get_stack_level_source(0) + ":" + itos(script->debug_get_stack_level_line(0)) + ":" + script->debug_get_stack_level_function(0) + "()";
			}
		} else {
			s += "\n   At: Cannot retrieve debug info outside the main thread. Thread ID: " + itos(Thread::get_caller_id());
		}

		print_line(s);
		*r_ret = Variant();
	}

	static inline void print_stack(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(0, 0);

		if (Thread::get_caller_id() != Thread::get_main_id()) {
			print_line("Cannot retrieve debug info outside the main thread. Thread ID: " + itos(Thread::get_caller_id()));
			return;
		}

		ScriptLanguage *script = GDScriptLanguage::get_singleton();
		for (int i = 0; i < script->debug_get_stack_level_count(); i++) {
			print_line("Frame " + itos(i) + " - " + script->debug_get_stack_level_source(i) + ":" + itos(script->debug_get_stack_level_line(i)) + " in function '" + script->debug_get_stack_level_function(i) + "'");
		};
		*r_ret = Variant();
	}

	static inline void get_stack(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(0, 0);

		if (Thread::get_caller_id() != Thread::get_main_id()) {
			*r_ret = TypedArray<Dictionary>();
			return;
		}

		ScriptLanguage *script = GDScriptLanguage::get_singleton();
		TypedArray<Dictionary> ret;
		for (int i = 0; i < script->debug_get_stack_level_count(); i++) {
			Dictionary frame;
			frame["source"] = script->debug_get_stack_level_source(i);
			frame["function"] = script->debug_get_stack_level_function(i);
			frame["line"] = script->debug_get_stack_level_line(i);
			ret.push_back(frame);
		};
		*r_ret = ret;
	}

	static inline void len(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(1, 1);
		switch (p_args[0]->get_type()) {
			case Variant::STRING:
			case Variant::STRING_NAME: {
				String d = *p_args[0];
				*r_ret = d.length();
			} break;
			case Variant::DICTIONARY: {
				Dictionary d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::ARRAY: {
				Array d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_BYTE_ARRAY: {
				Vector<uint8_t> d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_INT32_ARRAY: {
				Vector<int32_t> d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_INT64_ARRAY: {
				Vector<int64_t> d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_FLOAT32_ARRAY: {
				Vector<float> d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_FLOAT64_ARRAY: {
				Vector<double> d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_STRING_ARRAY: {
				Vector<String> d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_VECTOR2_ARRAY: {
				Vector<Vector2> d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_VECTOR3_ARRAY: {
				Vector<Vector3> d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_COLOR_ARRAY: {
				Vector<Color> d = *p_args[0];
				*r_ret = d.size();
			} break;
			case Variant::PACKED_VECTOR4_ARRAY: {
				Vector<Vector4> d = *p_args[0];
				*r_ret = d.size();
			} break;
			default: {
				*r_ret = vformat(RTR("Value of type '%s' can't provide a length."), Variant::get_type_name(p_args[0]->get_type()));
				r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
				r_error.argument = 0;
				r_error.expected = Variant::NIL;
			} break;
		}
	}

	static inline void is_instance_of(Variant *r_ret, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
		DEBUG_VALIDATE_ARG_COUNT(2, 2);

		if (p_args[1]->get_type() == Variant::INT) {
			int builtin_type = *p_args[1];
			DEBUG_VALIDATE_ARG_CUSTOM(1, Variant::NIL, builtin_type < 0 || builtin_type >= Variant::VARIANT_MAX,
					RTR("Invalid type argument for is_instance_of(), use TYPE_* constants for built-in types."));
			*r_ret = p_args[0]->get_type() == builtin_type;
			return;
		}

		bool was_type_freed = false;
		Object *type_object = p_args[1]->get_validated_object_with_check(was_type_freed);
		VALIDATE_ARG_CUSTOM(1, Variant::NIL, was_type_freed, RTR("Type argument is a previously freed instance."));
		VALIDATE_ARG_CUSTOM(1, Variant::NIL, !type_object,
				RTR("Invalid type argument for is_instance_of(), should be a TYPE_* constant, a class or a script."));

		bool was_value_freed = false;
		Object *value_object = p_args[0]->get_validated_object_with_check(was_value_freed);
		VALIDATE_ARG_CUSTOM(0, Variant::NIL, was_value_freed, RTR("Value argument is a previously freed instance."));
		if (!value_object) {
			*r_ret = false;
			return;
		}

		GDScriptNativeClass *native_type = Object::cast_to<GDScriptNativeClass>(type_object);
		if (native_type) {
			*r_ret = ClassDB::is_parent_class(value_object->get_class_name(), native_type->get_name());
			return;
		}

		Script *script_type = Object::cast_to<Script>(type_object);
		if (script_type) {
			bool result = false;
			if (value_object->get_script_instance()) {
				Script *script_ptr = value_object->get_script_instance()->get_script().ptr();
				while (script_ptr) {
					if (script_ptr == script_type) {
						result = true;
						break;
					}
					script_ptr = script_ptr->get_base_script().ptr();
				}
			}
			*r_ret = result;
			return;
		}

		*r_ret = RTR("Invalid type argument for is_instance_of(), should be a TYPE_* constant, a class or a script.");
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = Variant::NIL;
	}
};

struct GDScriptUtilityFunctionInfo {
	GDScriptUtilityFunctions::FunctionPtr function = nullptr;
	MethodInfo info;
	bool is_constant = false;
};

static OAHashMap<StringName, GDScriptUtilityFunctionInfo> utility_function_table;
static List<StringName> utility_function_name_table;

static void _register_function(const StringName &p_name, const MethodInfo &p_method_info, GDScriptUtilityFunctions::FunctionPtr p_function, bool p_is_const) {
	ERR_FAIL_COND(utility_function_table.has(p_name));

	GDScriptUtilityFunctionInfo function;
	function.function = p_function;
	function.info = p_method_info;
	function.is_constant = p_is_const;

	utility_function_table.insert(p_name, function);
	utility_function_name_table.push_back(p_name);
}

#define REGISTER_FUNC(m_func, m_is_const, m_return, m_args, m_is_vararg, m_default_args)         \
	{                                                                                            \
		String name(#m_func);                                                                    \
		if (name.begins_with("_")) {                                                             \
			name = name.substr(1);                                                               \
		}                                                                                        \
		MethodInfo info = m_args;                                                                \
		info.name = name;                                                                        \
		info.return_val = m_return;                                                              \
		info.default_arguments = m_default_args;                                                 \
		if (m_is_vararg) {                                                                       \
			info.flags |= METHOD_FLAG_VARARG;                                                    \
		}                                                                                        \
		_register_function(name, info, GDScriptUtilityFunctionsDefinitions::m_func, m_is_const); \
	}

#define RET(m_type) \
	PropertyInfo(Variant::m_type, "")

#define RETVAR \
	PropertyInfo(Variant::NIL, "", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)

#define RETCLS(m_class) \
	PropertyInfo(Variant::OBJECT, "", PROPERTY_HINT_RESOURCE_TYPE, m_class)

#define NOARGS \
	MethodInfo()

#define ARGS(...) \
	MethodInfo("", __VA_ARGS__)

#define ARG(m_name, m_type) \
	PropertyInfo(Variant::m_type, m_name)

#define ARGVAR(m_name) \
	PropertyInfo(Variant::NIL, m_name, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)

#define ARGTYPE(m_name) \
	PropertyInfo(Variant::INT, m_name, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_CLASS_IS_ENUM, "Variant.Type")

void GDScriptUtilityFunctions::register_functions() {
	/* clang-format off */
#ifndef DISABLE_DEPRECATED
	REGISTER_FUNC( convert,        true,  RETVAR,             ARGS( ARGVAR("what"), ARGTYPE("type") ), false, varray(     ));
#endif // DISABLE_DEPRECATED
	REGISTER_FUNC( type_exists,    true,  RET(BOOL),          ARGS( ARG("type", STRING_NAME)        ), false, varray(     ));
	REGISTER_FUNC( _char,          true,  RET(STRING),        ARGS( ARG("char", INT)                ), false, varray(     ));
	REGISTER_FUNC( range,          false, RET(ARRAY),         NOARGS,                                  true,  varray(     ));
	REGISTER_FUNC( load,           false, RETCLS("Resource"), ARGS( ARG("path", STRING)             ), false, varray(     ));
	REGISTER_FUNC( inst_to_dict,   false, RET(DICTIONARY),    ARGS( ARG("instance", OBJECT)         ), false, varray(     ));
	REGISTER_FUNC( dict_to_inst,   false, RET(OBJECT),        ARGS( ARG("dictionary", DICTIONARY)   ), false, varray(     ));
	REGISTER_FUNC( Color8,         true,  RET(COLOR),         ARGS( ARG("r8", INT), ARG("g8", INT),
																	ARG("b8", INT), ARG("a8", INT)  ), false, varray( 255 ));
	REGISTER_FUNC( print_debug,    false, RET(NIL),           NOARGS,                                  true,  varray(     ));
	REGISTER_FUNC( print_stack,    false, RET(NIL),           NOARGS,                                  false, varray(     ));
	REGISTER_FUNC( get_stack,      false, RET(ARRAY),         NOARGS,                                  false, varray(     ));
	REGISTER_FUNC( len,            true,  RET(INT),           ARGS( ARGVAR("var")                   ), false, varray(     ));
	REGISTER_FUNC( is_instance_of, true,  RET(BOOL),          ARGS( ARGVAR("value"), ARGVAR("type") ), false, varray(     ));
	/* clang-format on */
}

void GDScriptUtilityFunctions::unregister_functions() {
	utility_function_name_table.clear();
	utility_function_table.clear();
}

GDScriptUtilityFunctions::FunctionPtr GDScriptUtilityFunctions::get_function(const StringName &p_function) {
	GDScriptUtilityFunctionInfo *info = utility_function_table.lookup_ptr(p_function);
	ERR_FAIL_NULL_V(info, nullptr);
	return info->function;
}

bool GDScriptUtilityFunctions::has_function_return_value(const StringName &p_function) {
	GDScriptUtilityFunctionInfo *info = utility_function_table.lookup_ptr(p_function);
	ERR_FAIL_NULL_V(info, false);
	return info->info.return_val.type != Variant::NIL || bool(info->info.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT);
}

Variant::Type GDScriptUtilityFunctions::get_function_return_type(const StringName &p_function) {
	GDScriptUtilityFunctionInfo *info = utility_function_table.lookup_ptr(p_function);
	ERR_FAIL_NULL_V(info, Variant::NIL);
	return info->info.return_val.type;
}

StringName GDScriptUtilityFunctions::get_function_return_class(const StringName &p_function) {
	GDScriptUtilityFunctionInfo *info = utility_function_table.lookup_ptr(p_function);
	ERR_FAIL_NULL_V(info, StringName());
	return info->info.return_val.class_name;
}

Variant::Type GDScriptUtilityFunctions::get_function_argument_type(const StringName &p_function, int p_arg) {
	GDScriptUtilityFunctionInfo *info = utility_function_table.lookup_ptr(p_function);
	ERR_FAIL_NULL_V(info, Variant::NIL);
	ERR_FAIL_COND_V(p_arg >= info->info.arguments.size(), Variant::NIL);
	return info->info.arguments.get(p_arg).type;
}

int GDScriptUtilityFunctions::get_function_argument_count(const StringName &p_function) {
	GDScriptUtilityFunctionInfo *info = utility_function_table.lookup_ptr(p_function);
	ERR_FAIL_NULL_V(info, 0);
	return info->info.arguments.size();
}

bool GDScriptUtilityFunctions::is_function_vararg(const StringName &p_function) {
	GDScriptUtilityFunctionInfo *info = utility_function_table.lookup_ptr(p_function);
	ERR_FAIL_NULL_V(info, false);
	return (bool)(info->info.flags & METHOD_FLAG_VARARG);
}

bool GDScriptUtilityFunctions::is_function_constant(const StringName &p_function) {
	GDScriptUtilityFunctionInfo *info = utility_function_table.lookup_ptr(p_function);
	ERR_FAIL_NULL_V(info, false);
	return info->is_constant;
}

bool GDScriptUtilityFunctions::function_exists(const StringName &p_function) {
	return utility_function_table.has(p_function);
}

void GDScriptUtilityFunctions::get_function_list(List<StringName> *r_functions) {
	for (const StringName &E : utility_function_name_table) {
		r_functions->push_back(E);
	}
}

MethodInfo GDScriptUtilityFunctions::get_function_info(const StringName &p_function) {
	GDScriptUtilityFunctionInfo *info = utility_function_table.lookup_ptr(p_function);
	ERR_FAIL_NULL_V(info, MethodInfo());
	return info->info;
}
