/**************************************************************************/
/*  class_db.h                                                            */
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

#include "core/object/method_bind.h"
#include "core/object/object.h"
#include "core/string/print_string.h"

// Makes callable_mp readily available in all classes connecting signals.
// Needs to come after method_bind and object have been included.
#include "core/object/callable_method_pointer.h"
#include "core/templates/hash_set.h"

#include <type_traits>

#define DEFVAL(m_defval) (m_defval)
#define DEFVAL_ARRAY DEFVAL(ClassDB::default_array_arg)

#ifdef DEBUG_METHODS_ENABLED

struct MethodDefinition {
	StringName name;
	Vector<StringName> args;
	MethodDefinition() {}
	MethodDefinition(const char *p_name) :
			name(p_name) {}
	MethodDefinition(const StringName &p_name) :
			name(p_name) {}
};

MethodDefinition D_METHODP(const char *p_name, const char *const **p_args, uint32_t p_argcount);

template <typename... VarArgs>
MethodDefinition D_METHOD(const char *p_name, const VarArgs... p_args) {
	const char *args[sizeof...(p_args) + 1] = { p_args..., nullptr }; // +1 makes sure zero sized arrays are also supported.
	const char *const *argptrs[sizeof...(p_args) + 1];
	for (uint32_t i = 0; i < sizeof...(p_args); i++) {
		argptrs[i] = &args[i];
	}

	return D_METHODP(p_name, sizeof...(p_args) == 0 ? nullptr : (const char *const **)argptrs, sizeof...(p_args));
}

#else

// When DEBUG_METHODS_ENABLED is set this will let the engine know
// the argument names for easier debugging.
#define D_METHOD(m_c, ...) m_c

#endif

class ClassDB {
public:
	enum APIType {
		API_CORE,
		API_EDITOR,
		API_EXTENSION,
		API_EDITOR_EXTENSION,
		API_NONE
	};

public:
	struct PropertySetGet {
		int index;
		StringName setter;
		StringName getter;
		MethodBind *_setptr = nullptr;
		MethodBind *_getptr = nullptr;
		Variant::Type type;
	};

	struct ClassInfo {
		APIType api = API_NONE;
		ClassInfo *inherits_ptr = nullptr;
		void *class_ptr = nullptr;

		ObjectGDExtension *gdextension = nullptr;

		HashMap<StringName, MethodBind *> method_map;
		HashMap<StringName, LocalVector<MethodBind *>> method_map_compatibility;
		HashMap<StringName, int64_t> constant_map;
		struct EnumInfo {
			List<StringName> constants;
			bool is_bitfield = false;
		};

		HashMap<StringName, EnumInfo> enum_map;
		HashMap<StringName, MethodInfo> signal_map;
		List<PropertyInfo> property_list;
		HashMap<StringName, PropertyInfo> property_map;
#ifdef DEBUG_METHODS_ENABLED
		List<StringName> constant_order;
		List<StringName> method_order;
		HashSet<StringName> methods_in_properties;
		List<MethodInfo> virtual_methods;
		HashMap<StringName, MethodInfo> virtual_methods_map;
		HashMap<StringName, Vector<Error>> method_error_values;
		HashMap<StringName, List<StringName>> linked_properties;
#endif
		HashMap<StringName, PropertySetGet> property_setget;
		HashMap<StringName, Vector<uint32_t>> virtual_methods_compat;

		StringName inherits;
		StringName name;
		bool disabled = false;
		bool exposed = false;
		bool reloadable = false;
		bool is_virtual = false;
		bool is_runtime = false;
		// The bool argument indicates the need to postinitialize.
		Object *(*creation_func)(bool) = nullptr;

		ClassInfo() {}
		~ClassInfo() {}
	};

	template <typename T>
	static Object *creator(bool p_notify_postinitialize) {
		Object *ret = new ("") T;
		ret->_initialize();
		if (p_notify_postinitialize) {
			ret->_postinitialize();
		}
		return ret;
	}

	static RWLock lock;
	static HashMap<StringName, ClassInfo> classes;
	static HashMap<StringName, StringName> resource_base_extensions;
	static HashMap<StringName, StringName> compat_classes;

#ifdef TOOLS_ENABLED
	static HashMap<StringName, ObjectGDExtension> placeholder_extensions;
#endif

#ifdef DEBUG_METHODS_ENABLED
	static MethodBind *bind_methodfi(uint32_t p_flags, MethodBind *p_bind, bool p_compatibility, const MethodDefinition &method_name, const Variant **p_defs, int p_defcount);
#else
	static MethodBind *bind_methodfi(uint32_t p_flags, MethodBind *p_bind, bool p_compatibility, const char *method_name, const Variant **p_defs, int p_defcount);
#endif

	static APIType current_api;
	static HashMap<APIType, uint32_t> api_hashes_cache;

	static void _add_class2(const StringName &p_class, const StringName &p_inherits);

	static HashMap<StringName, HashMap<StringName, Variant>> default_values;
	static HashSet<StringName> default_values_cached;

	// Native structs, used by binder
	struct NativeStruct {
		String ccode; // C code to create the native struct, fields separated by ; Arrays accepted (even containing other structs), also function pointers. All types must be Godot types.
		uint64_t struct_size; // local size of struct, for comparison
	};
	static HashMap<StringName, NativeStruct> native_structs;

	static Array default_array_arg;
	static bool is_default_array_arg(const Array &p_array);

private:
	// Non-locking variants of get_parent_class and is_parent_class.
	static StringName _get_parent_class(const StringName &p_class);
	static bool _is_parent_class(const StringName &p_class, const StringName &p_inherits);
	static void _bind_compatibility(ClassInfo *type, MethodBind *p_method);
	static MethodBind *_bind_vararg_method(MethodBind *p_bind, const StringName &p_name, const Vector<Variant> &p_default_args, bool p_compatibility);
	static void _bind_method_custom(const StringName &p_class, MethodBind *p_method, bool p_compatibility);

	static Object *_instantiate_internal(const StringName &p_class, bool p_require_real_class = false, bool p_notify_postinitialize = true);

	static bool _can_instantiate(ClassInfo *p_class_info);

public:
	// DO NOT USE THIS!!!!!! NEEDS TO BE PUBLIC BUT DO NOT USE NO MATTER WHAT!!!
	template <typename T>
	static void _add_class() {
		_add_class2(T::get_class_static(), T::get_parent_class_static());
	}

	template <typename T>
	static void register_class(bool p_virtual = false) {
		GLOBAL_LOCK_FUNCTION;
		static_assert(std::is_same_v<typename T::self_type, T>, "Class not declared properly, please use GDCLASS.");
		T::initialize_class();
		ClassInfo *t = classes.getptr(T::get_class_static());
		ERR_FAIL_NULL(t);
		t->creation_func = &creator<T>;
		t->exposed = true;
		t->is_virtual = p_virtual;
		t->class_ptr = T::get_class_ptr_static();
		t->api = current_api;
		T::register_custom_data_to_otdb();
	}

	template <typename T>
	static void register_abstract_class() {
		GLOBAL_LOCK_FUNCTION;
		static_assert(std::is_same_v<typename T::self_type, T>, "Class not declared properly, please use GDCLASS.");
		T::initialize_class();
		ClassInfo *t = classes.getptr(T::get_class_static());
		ERR_FAIL_NULL(t);
		t->exposed = true;
		t->class_ptr = T::get_class_ptr_static();
		t->api = current_api;
		//nothing
	}

	template <typename T>
	static void register_internal_class() {
		GLOBAL_LOCK_FUNCTION;
		static_assert(std::is_same_v<typename T::self_type, T>, "Class not declared properly, please use GDCLASS.");
		T::initialize_class();
		ClassInfo *t = classes.getptr(T::get_class_static());
		ERR_FAIL_NULL(t);
		t->creation_func = &creator<T>;
		t->exposed = false;
		t->is_virtual = false;
		t->class_ptr = T::get_class_ptr_static();
		t->api = current_api;
		T::register_custom_data_to_otdb();
	}

	template <typename T>
	static void register_runtime_class() {
		GLOBAL_LOCK_FUNCTION;
		static_assert(std::is_same_v<typename T::self_type, T>, "Class not declared properly, please use GDCLASS.");
		T::initialize_class();
		ClassInfo *t = classes.getptr(T::get_class_static());
		ERR_FAIL_NULL(t);
		ERR_FAIL_COND_MSG(t->inherits_ptr && !t->inherits_ptr->creation_func, vformat("Cannot register runtime class '%s' that descends from an abstract parent class.", T::get_class_static()));
		t->creation_func = &creator<T>;
		t->exposed = true;
		t->is_virtual = false;
		t->is_runtime = true;
		t->class_ptr = T::get_class_ptr_static();
		t->api = current_api;
		T::register_custom_data_to_otdb();
	}

	static void register_extension_class(ObjectGDExtension *p_extension);
	static void unregister_extension_class(const StringName &p_class, bool p_free_method_binds = true);

	template <typename T>
	static Object *_create_ptr_func(bool p_notify_postinitialize) {
		return T::create(p_notify_postinitialize);
	}

	template <typename T>
	static void register_custom_instance_class() {
		GLOBAL_LOCK_FUNCTION;
		static_assert(std::is_same_v<typename T::self_type, T>, "Class not declared properly, please use GDCLASS.");
		T::initialize_class();
		ClassInfo *t = classes.getptr(T::get_class_static());
		ERR_FAIL_NULL(t);
		t->creation_func = &_create_ptr_func<T>;
		t->exposed = true;
		t->class_ptr = T::get_class_ptr_static();
		t->api = current_api;
		T::register_custom_data_to_otdb();
	}

	static void get_class_list(List<StringName> *p_classes);
#ifdef TOOLS_ENABLED
	static void get_extensions_class_list(List<StringName> *p_classes);
	static void get_extension_class_list(const Ref<GDExtension> &p_extension, List<StringName> *p_classes);
	static ObjectGDExtension *get_placeholder_extension(const StringName &p_class);
#endif
	static void get_inheriters_from_class(const StringName &p_class, List<StringName> *p_classes);
	static void get_direct_inheriters_from_class(const StringName &p_class, List<StringName> *p_classes);
	static StringName get_parent_class_nocheck(const StringName &p_class);
	static bool get_inheritance_chain_nocheck(const StringName &p_class, Vector<StringName> &r_result);
	static StringName get_parent_class(const StringName &p_class);
	static StringName get_compatibility_remapped_class(const StringName &p_class);
	static bool class_exists(const StringName &p_class);
	static bool is_parent_class(const StringName &p_class, const StringName &p_inherits);
	static bool can_instantiate(const StringName &p_class);
	static bool is_abstract(const StringName &p_class);
	static bool is_virtual(const StringName &p_class);
	static Object *instantiate(const StringName &p_class);
	static Object *instantiate_no_placeholders(const StringName &p_class);
	static Object *instantiate_without_postinitialization(const StringName &p_class);
	static void set_object_extension_instance(Object *p_object, const StringName &p_class, GDExtensionClassInstancePtr p_instance);

	static APIType get_api_type(const StringName &p_class);

	static uint32_t get_api_hash(APIType p_api);

	template <typename>
	struct member_function_traits;

	template <typename R, typename T, typename... Args>
	struct member_function_traits<R (T::*)(Args...)> {
		using return_type = R;
	};

	template <typename R, typename T, typename... Args>
	struct member_function_traits<R (T::*)(Args...) const> {
		using return_type = R;
	};

	template <typename R, typename... Args>
	struct member_function_traits<R (*)(Args...)> {
		using return_type = R;
	};

	template <typename N, typename M, typename... VarArgs>
	static MethodBind *bind_method(N p_method_name, M p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		MethodBind *bind = create_method_bind(p_method);
		if constexpr (std::is_same_v<typename member_function_traits<M>::return_type, Object *>) {
			bind->set_return_type_is_raw_object_ptr(true);
		}
		return bind_methodfi(METHOD_FLAGS_DEFAULT, bind, false, p_method_name, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}

	template <typename N, typename M, typename... VarArgs>
	static MethodBind *bind_static_method(const StringName &p_class, N p_method_name, M p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		MethodBind *bind = create_static_method_bind(p_method);
		bind->set_instance_class(p_class);
		if constexpr (std::is_same_v<typename member_function_traits<M>::return_type, Object *>) {
			bind->set_return_type_is_raw_object_ptr(true);
		}
		return bind_methodfi(METHOD_FLAGS_DEFAULT, bind, false, p_method_name, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}

	template <typename N, typename M, typename... VarArgs>
	static MethodBind *bind_compatibility_method(N p_method_name, M p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		MethodBind *bind = create_method_bind(p_method);
		if constexpr (std::is_same_v<typename member_function_traits<M>::return_type, Object *>) {
			bind->set_return_type_is_raw_object_ptr(true);
		}
		return bind_methodfi(METHOD_FLAGS_DEFAULT, bind, true, p_method_name, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}

	template <typename N, typename M, typename... VarArgs>
	static MethodBind *bind_compatibility_static_method(const StringName &p_class, N p_method_name, M p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		MethodBind *bind = create_static_method_bind(p_method);
		bind->set_instance_class(p_class);
		if constexpr (std::is_same_v<typename member_function_traits<M>::return_type, Object *>) {
			bind->set_return_type_is_raw_object_ptr(true);
		}
		return bind_methodfi(METHOD_FLAGS_DEFAULT, bind, true, p_method_name, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}

	template <typename M>
	static MethodBind *bind_vararg_method(uint32_t p_flags, const StringName &p_name, M p_method, const MethodInfo &p_info = MethodInfo(), const Vector<Variant> &p_default_args = Vector<Variant>(), bool p_return_nil_is_variant = true) {
		GLOBAL_LOCK_FUNCTION;

		MethodBind *bind = create_vararg_method_bind(p_method, p_info, p_return_nil_is_variant);
		ERR_FAIL_NULL_V(bind, nullptr);

		if constexpr (std::is_same_v<typename member_function_traits<M>::return_type, Object *>) {
			bind->set_return_type_is_raw_object_ptr(true);
		}
		return _bind_vararg_method(bind, p_name, p_default_args, false);
	}

	template <typename M>
	static MethodBind *bind_compatibility_vararg_method(uint32_t p_flags, const StringName &p_name, M p_method, const MethodInfo &p_info = MethodInfo(), const Vector<Variant> &p_default_args = Vector<Variant>(), bool p_return_nil_is_variant = true) {
		GLOBAL_LOCK_FUNCTION;

		MethodBind *bind = create_vararg_method_bind(p_method, p_info, p_return_nil_is_variant);
		ERR_FAIL_NULL_V(bind, nullptr);

		if constexpr (std::is_same_v<typename member_function_traits<M>::return_type, Object *>) {
			bind->set_return_type_is_raw_object_ptr(true);
		}
		return _bind_vararg_method(bind, p_name, p_default_args, true);
	}

	static void bind_method_custom(const StringName &p_class, MethodBind *p_method);
	static void bind_compatibility_method_custom(const StringName &p_class, MethodBind *p_method);

	static void add_signal(const StringName &p_class, const MethodInfo &p_signal);
	static bool has_signal(const StringName &p_class, const StringName &p_signal, bool p_no_inheritance = false);
	static bool get_signal(const StringName &p_class, const StringName &p_signal, MethodInfo *r_signal);
	static void get_signal_list(const StringName &p_class, List<MethodInfo> *p_signals, bool p_no_inheritance = false);

	static void add_property_group(const StringName &p_class, const String &p_name, const String &p_prefix = "", int p_indent_depth = 0);
	static void add_property_subgroup(const StringName &p_class, const String &p_name, const String &p_prefix = "", int p_indent_depth = 0);
	static void add_property_array_count(const StringName &p_class, const String &p_label, const StringName &p_count_property, const StringName &p_count_setter, const StringName &p_count_getter, const String &p_array_element_prefix, uint32_t p_count_usage = PROPERTY_USAGE_DEFAULT);
	static void add_property_array(const StringName &p_class, const StringName &p_path, const String &p_array_element_prefix);
	static void add_property(const StringName &p_class, const PropertyInfo &p_pinfo, const StringName &p_setter, const StringName &p_getter, int p_index = -1);
	static void set_property_default_value(const StringName &p_class, const StringName &p_name, const Variant &p_default);
	static void add_linked_property(const StringName &p_class, const String &p_property, const String &p_linked_property);
	static void get_property_list(const StringName &p_class, List<PropertyInfo> *p_list, bool p_no_inheritance = false, const Object *p_validator = nullptr);
	static bool get_property_info(const StringName &p_class, const StringName &p_property, PropertyInfo *r_info, bool p_no_inheritance = false, const Object *p_validator = nullptr);
	static void get_linked_properties_info(const StringName &p_class, const StringName &p_property, List<StringName> *r_properties, bool p_no_inheritance = false);
	static bool set_property(Object *p_object, const StringName &p_property, const Variant &p_value, bool *r_valid = nullptr);
	static bool get_property(Object *p_object, const StringName &p_property, Variant &r_value);
	static bool has_property(const StringName &p_class, const StringName &p_property, bool p_no_inheritance = false);
	static int get_property_index(const StringName &p_class, const StringName &p_property, bool *r_is_valid = nullptr);
	static Variant::Type get_property_type(const StringName &p_class, const StringName &p_property, bool *r_is_valid = nullptr);
	static StringName get_property_setter(const StringName &p_class, const StringName &p_property);
	static StringName get_property_getter(const StringName &p_class, const StringName &p_property);

	static bool has_method(const StringName &p_class, const StringName &p_method, bool p_no_inheritance = false);
	static void set_method_flags(const StringName &p_class, const StringName &p_method, int p_flags);

	static void get_method_list(const StringName &p_class, List<MethodInfo> *p_methods, bool p_no_inheritance = false, bool p_exclude_from_properties = false);
	static void get_method_list_with_compatibility(const StringName &p_class, List<Pair<MethodInfo, uint32_t>> *p_methods_with_hash, bool p_no_inheritance = false, bool p_exclude_from_properties = false);
	static bool get_method_info(const StringName &p_class, const StringName &p_method, MethodInfo *r_info, bool p_no_inheritance = false, bool p_exclude_from_properties = false);
	static int get_method_argument_count(const StringName &p_class, const StringName &p_method, bool *r_is_valid = nullptr, bool p_no_inheritance = false);
	static MethodBind *get_method(const StringName &p_class, const StringName &p_name);
	static MethodBind *get_method_with_compatibility(const StringName &p_class, const StringName &p_name, uint64_t p_hash, bool *r_method_exists = nullptr, bool *r_is_deprecated = nullptr);
	static Vector<uint32_t> get_method_compatibility_hashes(const StringName &p_class, const StringName &p_name);

	static void add_virtual_method(const StringName &p_class, const MethodInfo &p_method, bool p_virtual = true, const Vector<String> &p_arg_names = Vector<String>(), bool p_object_core = false);
	static void add_virtual_compatibility_method(const StringName &p_class, const MethodInfo &p_method, bool p_virtual = true, const Vector<String> &p_arg_names = Vector<String>(), bool p_object_core = false);
	static void get_virtual_methods(const StringName &p_class, List<MethodInfo> *p_methods, bool p_no_inheritance = false);
	static void add_extension_class_virtual_method(const StringName &p_class, const GDExtensionClassVirtualMethodInfo *p_method_info);
	static Vector<uint32_t> get_virtual_method_compatibility_hashes(const StringName &p_class, const StringName &p_name);

	static void bind_integer_constant(const StringName &p_class, const StringName &p_enum, const StringName &p_name, int64_t p_constant, bool p_is_bitfield = false);
	static void get_integer_constant_list(const StringName &p_class, List<String> *p_constants, bool p_no_inheritance = false);
	static int64_t get_integer_constant(const StringName &p_class, const StringName &p_name, bool *p_success = nullptr);
	static bool has_integer_constant(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false);

	static StringName get_integer_constant_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false);
	static StringName get_integer_constant_bitfield(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false);
	static void get_enum_list(const StringName &p_class, List<StringName> *p_enums, bool p_no_inheritance = false);
	static void get_enum_constants(const StringName &p_class, const StringName &p_enum, List<StringName> *p_constants, bool p_no_inheritance = false);
	static bool has_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false);
	static bool is_enum_bitfield(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false);

	static void set_method_error_return_values(const StringName &p_class, const StringName &p_method, const Vector<Error> &p_values);
	static Vector<Error> get_method_error_return_values(const StringName &p_class, const StringName &p_method);
	static Variant class_get_default_property_value(const StringName &p_class, const StringName &p_property, bool *r_valid = nullptr);

	static void set_class_enabled(const StringName &p_class, bool p_enable);
	static bool is_class_enabled(const StringName &p_class);

	static bool is_class_exposed(const StringName &p_class);
	static bool is_class_reloadable(const StringName &p_class);
	static bool is_class_runtime(const StringName &p_class);

	static void add_resource_base_extension(const StringName &p_extension, const StringName &p_class);
	static void get_resource_base_extensions(List<String> *p_extensions);
	static void get_extensions_for_type(const StringName &p_class, List<String> *p_extensions);
	static bool is_resource_extension(const StringName &p_extension);

	static void add_compatibility_class(const StringName &p_class, const StringName &p_fallback);
	static StringName get_compatibility_class(const StringName &p_class);

	static void set_current_api(APIType p_api);
	static APIType get_current_api();
	static void cleanup_defaults();
	static void cleanup();

	static void register_native_struct(const StringName &p_name, const String &p_code, uint64_t p_current_size);
	static void get_native_struct_list(List<StringName> *r_names);
	static String get_native_struct_code(const StringName &p_name);
	static uint64_t get_native_struct_size(const StringName &p_name); // Used for asserting
};

#define BIND_ENUM_CONSTANT(m_constant) \
	::ClassDB::bind_integer_constant(get_class_static(), __constant_get_enum_name(m_constant, #m_constant), #m_constant, m_constant);

#define BIND_BITFIELD_FLAG(m_constant) \
	::ClassDB::bind_integer_constant(get_class_static(), __constant_get_bitfield_name(m_constant, #m_constant), #m_constant, m_constant, true);

#define BIND_CONSTANT(m_constant) \
	::ClassDB::bind_integer_constant(get_class_static(), StringName(), #m_constant, m_constant);

#ifdef DEBUG_METHODS_ENABLED

_FORCE_INLINE_ void errarray_add_str(Vector<Error> &arr) {
}

_FORCE_INLINE_ void errarray_add_str(Vector<Error> &arr, const Error &p_err) {
	arr.push_back(p_err);
}

template <typename... P>
_FORCE_INLINE_ void errarray_add_str(Vector<Error> &arr, const Error &p_err, P... p_args) {
	arr.push_back(p_err);
	errarray_add_str(arr, p_args...);
}

template <typename... P>
_FORCE_INLINE_ Vector<Error> errarray(P... p_args) {
	Vector<Error> arr;
	errarray_add_str(arr, p_args...);
	return arr;
}

#define BIND_METHOD_ERR_RETURN_DOC(m_method, ...) \
	::ClassDB::set_method_error_return_values(get_class_static(), m_method, errarray(__VA_ARGS__));

#else

#define BIND_METHOD_ERR_RETURN_DOC(m_method, ...)

#endif

#define GDREGISTER_CLASS(m_class)             \
	if (m_class::_class_is_enabled) {         \
		::ClassDB::register_class<m_class>(); \
	}
#define GDREGISTER_VIRTUAL_CLASS(m_class)         \
	if (m_class::_class_is_enabled) {             \
		::ClassDB::register_class<m_class>(true); \
	}
#define GDREGISTER_ABSTRACT_CLASS(m_class)             \
	if (m_class::_class_is_enabled) {                  \
		::ClassDB::register_abstract_class<m_class>(); \
	}
#define GDREGISTER_INTERNAL_CLASS(m_class)             \
	if (m_class::_class_is_enabled) {                  \
		::ClassDB::register_internal_class<m_class>(); \
	}

#define GDREGISTER_RUNTIME_CLASS(m_class)             \
	if (m_class::_class_is_enabled) {                 \
		::ClassDB::register_runtime_class<m_class>(); \
	}

#define GDREGISTER_NATIVE_STRUCT(m_class, m_code) ClassDB::register_native_struct(#m_class, m_code, sizeof(m_class))

#include "core/disabled_classes.gen.h"
