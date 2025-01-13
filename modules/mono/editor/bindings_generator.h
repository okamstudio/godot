/**************************************************************************/
/*  bindings_generator.h                                                  */
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

#ifndef BINDINGS_GENERATOR_H
#define BINDINGS_GENERATOR_H

#include "core/typedefs.h" // DEBUG_METHODS_ENABLED

#if defined(DEBUG_METHODS_ENABLED) && defined(TOOLS_ENABLED)

#include "core/doc_data.h"
#include "core/object/class_db.h"
#include "core/string/string_builder.h"
#include "core/string/ustring.h"
#include "editor/doc_tools.h"
#include "editor/editor_help.h"

class BindingsGenerator {
	struct ConstantInterface {
		String name;
		String proxy_name;
		int64_t value = 0;
		const DocData::ConstantDoc *const_doc = nullptr;

		bool is_deprecated = false;
		String deprecation_message;

		ConstantInterface() {}

		ConstantInterface(const String &p_name, const String &p_proxy_name, int64_t p_value) {
			name = p_name;
			proxy_name = p_proxy_name;
			value = p_value;
		}
	};

	struct EnumInterface {
		StringName cname;
		String proxy_name;
		List<ConstantInterface> constants;
		bool is_flags = false;

		_FORCE_INLINE_ bool operator==(const EnumInterface &p_ienum) const {
			return p_ienum.cname == cname;
		}

		EnumInterface() {}

		EnumInterface(const StringName &p_cname, const String &p_proxy_name, bool p_is_flags) {
			cname = p_cname;
			proxy_name = p_proxy_name;
			is_flags = p_is_flags;
		}
	};

	struct PropertyInterface {
		StringName cname;
		String proxy_name;
		int index = 0;

		StringName setter;
		StringName getter;

		/**
		 * Determines if the property will be hidden with the [EditorBrowsable(EditorBrowsableState.Never)]
		 * attribute.
		 * We do this for propertyies that have the PROPERTY_USAGE_INTERNAL flag, because they are not meant
		 * to be exposed to scripting but we can't remove them to prevent breaking compatibility.
		 */
		bool is_hidden = false;

		const DocData::PropertyDoc *prop_doc;

		bool is_deprecated = false;
		String deprecation_message;
	};

	struct TypeReference {
		StringName cname;
		bool is_enum = false;

		List<TypeReference> generic_type_parameters;

		TypeReference() {}

		TypeReference(const StringName &p_cname) :
				cname(p_cname) {}
	};

	struct ArgumentInterface {
		enum DefaultParamMode {
			CONSTANT,
			NULLABLE_VAL,
			NULLABLE_REF
		};

		TypeReference type;

		String name;

		Variant def_param_value;
		DefaultParamMode def_param_mode = CONSTANT;

		/**
		 * Determines the expression for the parameter default value.
		 * Formatting elements:
		 * %0 or %s: [cs_type] of the argument type
		 */
		String default_argument;

		ArgumentInterface() {}
	};

	struct MethodInterface {
		String name;
		StringName cname;

		/**
		 * Name of the C# method
		 */
		String proxy_name;

		/**
		 * Hash of the ClassDB method
		 */
		uint64_t hash = 0;

		/**
		 * [TypeInterface::name] of the return type
		 */
		TypeReference return_type;

		/**
		 * Determines if the method has a variable number of arguments (VarArg)
		 */
		bool is_vararg = false;

		/**
		 * Determines if the method is static.
		 */
		bool is_static = false;

		/**
		 * Virtual methods ("virtual" as defined by the Godot API) are methods that by default do nothing,
		 * but can be overridden by the user to add custom functionality.
		 * e.g.: _ready, _process, etc.
		 */
		bool is_virtual = false;

		/**
		 * Determines if the call should fallback to Godot's object.Call(string, params) in C#.
		 */
		bool requires_object_call = false;

		/**
		 * Determines if the method visibility is 'internal' (visible only to files in the same assembly).
		 * Currently, we only use this for methods that are not meant to be exposed,
		 * but are required by properties as getters or setters.
		 * Methods that are not meant to be exposed are those that begin with underscore and are not virtual.
		 */
		bool is_internal = false;

		/**
		 * Determines if the method will be hidden with the [EditorBrowsable(EditorBrowsableState.Never)]
		 * attribute.
		 * We do this for methods that we don't want to expose but need to be public to prevent breaking
		 * compat (i.e: methods with 'is_compat' set to true.)
		 */
		bool is_hidden = false;

		/**
		 * Determines if the method is a compatibility method added to avoid breaking binary compatibility.
		 * These methods will be generated but hidden and are considered deprecated.
		 */
		bool is_compat = false;

		List<ArgumentInterface> arguments;

		const DocData::MethodDoc *method_doc = nullptr;

		bool is_deprecated = false;
		String deprecation_message;

		void add_argument(const ArgumentInterface &argument) {
			arguments.push_back(argument);
		}

		MethodInterface() {}
	};

	struct SignalInterface {
		String name;
		StringName cname;

		/**
		 * Name of the C# method
		 */
		String proxy_name;

		List<ArgumentInterface> arguments;

		const DocData::MethodDoc *method_doc = nullptr;

		bool is_deprecated = false;
		String deprecation_message;

		void add_argument(const ArgumentInterface &argument) {
			arguments.push_back(argument);
		}

		SignalInterface() {}
	};

	struct TypeInterface {
		/**
		 * Identifier name for this type.
		 * Also used to format [c_out].
		 */
		String name;
		StringName cname;

		int type_parameter_count = 0;

		/**
		 * Identifier name of the base class.
		 */
		StringName base_name;

		/**
		 * Name of the C# class
		 */
		String proxy_name;

		ClassDB::APIType api_type = ClassDB::API_NONE;

		bool is_enum = false;
		bool is_object_type = false;
		bool is_singleton = false;
		bool is_singleton_instance = false;
		bool is_ref_counted = false;
		bool is_span_compatible = false;

		/**
		 * Class is a singleton, but can't be declared as a static class as that would
		 * break backwards compatibility. As such, instead of going with a static class,
		 * we use the actual singleton pattern (private constructor with instance property),
		 * which doesn't break compatibility.
		 */
		bool is_compat_singleton = false;

		/**
		 * Determines whether the native return value of this type must be disposed
		 * by the generated internal call (think of `godot_string`, whose destructor
		 * must be called). Some structs that are disposable may still disable this
		 * flag if the ownership is transferred.
		 */
		bool c_type_is_disposable_struct = false;

		/**
		 * Determines whether the native return value of this type must be zero initialized
		 * before its address is passed to ptrcall. This is required for types whose destructor
		 * is called before being assigned the return value by `PtrToArg::encode`, e.g.:
		 * Array, Dictionary, String, StringName, Variant.
		 * It's not necessary to set this to `true` if [c_type_is_disposable_struct] is already `true`.
		 */
		bool c_ret_needs_default_initialization = false;

		/**
		 * Used only by Object-derived types.
		 * Determines if this type is not abstract (incomplete).
		 * e.g.: CanvasItem cannot be instantiated.
		 */
		bool is_instantiable = false;

		/**
		 * Used only by Object-derived types.
		 * Determines if the C# class owns the native handle and must free it somehow when disposed.
		 * e.g.: RefCounted types must notify when the C# instance is disposed, for proper refcounting.
		 */
		bool memory_own = false;

		// !! The comments of the following fields make reference to other fields via square brackets, e.g.: [field_name]
		// !! When renaming those fields, make sure to rename their references in the comments

		// --- C INTERFACE ---

		/**
		 * One or more statements that transform the parameter before being passed as argument of a ptrcall.
		 * If the statement adds a local that must be passed as the argument instead of the parameter,
		 * the expression with the name of that local must be specified with [c_arg_in].
		 * Formatting elements:
		 * %0: [c_type] of the parameter
		 * %1: name of the parameter
		 * %2-4: reserved
		 * %5: indentation text
		 */
		String c_in;

		/**
		 * One or more statements that transform the parameter before being passed as argument of a vararg call.
		 * If the statement adds a local that must be passed as the argument instead of the parameter,
		 * the name of that local must be specified with [c_arg_in].
		 * Formatting elements:
		 * %0: [c_type] of the parameter
		 * %1: name of the parameter
		 * %2-4: reserved
		 * %5: indentation text
		 */
		String c_in_vararg;

		/**
		 * Determines the expression that will be passed as argument to ptrcall.
		 * By default the value equals the name of the parameter,
		 * this varies for types that require special manipulation via [c_in].
		 * Formatting elements:
		 * %0 or %s: name of the parameter
		 */
		String c_arg_in = "%s";

		/**
		 * One or more statements that determine how a variable of this type is returned from a function.
		 * It must contain the return statement(s).
		 * Formatting elements:
		 * %0: [c_type_out] of the return type
		 * %1: name of the variable to be returned
		 * %2: [name] of the return type
		 * ---------------------------------------
		 * If [ret_as_byref_arg] is true, the format is different. Instead of using a return statement,
		 * the value must be assigned to a parameter. This type of this parameter is a pointer to [c_type_out].
		 * Formatting elements:
		 * %0: [c_type_out] of the return type
		 * %1: name of the variable to be returned
		 * %2: [name] of the return type
		 * %3-4: reserved
		 * %5: indentation text
		 */
		String c_out;

		/**
		 * The actual expected type, as seen (in most cases) in Variant copy constructors
		 * Used for the type of the return variable and to format [c_in].
		 * The value must be the following depending of the type:
		 * Object-derived types: Object*
		 * Other types: [name]
		 * -- Exceptions --
		 * VarArg (fictitious type to represent variable arguments): Array
		 * float: double (because ptrcall only supports double)
		 * int: int64_t (because ptrcall only supports int64_t and uint64_t)
		 * RefCounted types override this for the type of the return variable: Ref<RefCounted>
		 */
		String c_type;

		/**
		 * Determines the type used for parameters in function signatures.
		 */
		String c_type_in;

		/**
		 * Determines the return type used for function signatures.
		 * Also used to construct a default value to return in case of errors,
		 * and to format [c_out].
		 */
		String c_type_out;

		// --- C# INTERFACE ---

		/**
		 * An expression that overrides the way the parameter is passed to the internal call.
		 * If empty, the parameter is passed as is.
		 * Formatting elements:
		 * %0: name of the parameter
		 * %1: [c_type] of the parameter
		 */
		String cs_in_expr;
		bool cs_in_expr_is_unsafe = false;

		/**
		 * One or more statements that transform the parameter before being passed to the internal call.
		 * If the statement adds a local that must be passed as the argument instead of the parameter,
		 * the expression with the name of that local must be specified with [cs_in_expr].
		 * Formatting elements:
		 * %0: [c_type] of the parameter
		 * %1: name of the parameter
		 * %2-4: reserved
		 * %5: indentation text
		 */
		String cs_in;

		/**
		 * One or more statements that determine how a variable of this type is returned from a method.
		 * It must contain the return statement(s).
		 * Formatting elements:
		 * %0: internal method name
		 * %1: internal method call arguments without surrounding parenthesis
		 * %2: [cs_type] of the return type
		 * %3: [c_type_out] of the return type
		 * %4: reserved
		 * %5: indentation text
		 */
		String cs_out;

		/**
		 * Type used for method signatures, both for parameters and the return type.
		 * Same as [proxy_name] except for variable arguments (VarArg) and collections (which include the namespace).
		 */
		String cs_type;

		/**
		 * Formatting elements:
		 * %0: input expression of type `in godot_variant`
		 * %1: [cs_type] of this type
		 * %2: [name] of this type
		 */
		String cs_variant_to_managed;

		/**
		 * Formatting elements:
		 * %0: input expression
		 * %1: [cs_type] of this type
		 * %2: [name] of this type
		 */
		String cs_managed_to_variant;

		const DocData::ClassDoc *class_doc = nullptr;

		bool is_deprecated = false;
		String deprecation_message;

		List<ConstantInterface> constants;
		List<EnumInterface> enums;
		List<PropertyInterface> properties;
		List<MethodInterface> methods;
		List<SignalInterface> signals_;
		HashSet<String> ignored_members;

		bool has_virtual_methods = false;

		const MethodInterface *find_method_by_name(const StringName &p_cname) const {
			for (const MethodInterface &E : methods) {
				if (E.cname == p_cname) {
					return &E;
				}
			}

			return nullptr;
		}

		const MethodInterface *find_method_by_proxy_name(const String &p_proxy_name) const {
			for (const MethodInterface &E : methods) {
				if (E.proxy_name == p_proxy_name) {
					return &E;
				}
			}

			return nullptr;
		}

		const PropertyInterface *find_property_by_name(const StringName &p_cname) const {
			for (const PropertyInterface &E : properties) {
				if (E.cname == p_cname) {
					return &E;
				}
			}

			return nullptr;
		}

		const PropertyInterface *find_property_by_proxy_name(const String &p_proxy_name) const {
			for (const PropertyInterface &E : properties) {
				if (E.proxy_name == p_proxy_name) {
					return &E;
				}
			}

			return nullptr;
		}

		const SignalInterface *find_signal_by_name(const StringName &p_cname) const {
			for (const SignalInterface &E : signals_) {
				if (E.cname == p_cname) {
					return &E;
				}
			}

			return nullptr;
		}

		const SignalInterface *find_signal_by_proxy_name(const String &p_proxy_name) const {
			for (const SignalInterface &E : signals_) {
				if (E.proxy_name == p_proxy_name) {
					return &E;
				}
			}

			return nullptr;
		}

		bool is_intentionally_ignored(const String &p_name) const {
			return ignored_members.has(p_name);
		}

	private:
		static DocData::ClassDoc *_get_type_doc(TypeInterface &itype) {
			String doc_name = itype.name.begins_with("_") ? itype.name.substr(1) : itype.name;
			return &EditorHelp::get_doc_data()->class_list[doc_name];
		}

		static void _init_value_type(TypeInterface &itype) {
			if (itype.proxy_name.is_empty()) {
				itype.proxy_name = itype.name;
			}

			itype.cs_type = itype.proxy_name;
			itype.c_type = itype.cs_type;
			itype.c_type_in = itype.cs_type + "*";
			itype.c_type_out = itype.cs_type;

			itype.class_doc = _get_type_doc(itype);
		}

		static void _init_object_type(TypeInterface &itype, ClassDB::APIType p_api_type) {
			if (itype.proxy_name.is_empty()) {
				itype.proxy_name = itype.name;
			}

			if (itype.proxy_name.begins_with("_")) {
				itype.proxy_name = itype.proxy_name.substr(1);
			}

			itype.api_type = p_api_type;
			itype.is_object_type = true;

			itype.class_doc = _get_type_doc(itype);
		}

	public:
		static TypeInterface create_value_type(const String &p_name, const String &p_proxy_name) {
			TypeInterface itype;
			itype.name = p_name;
			itype.cname = p_name;
			itype.proxy_name = p_proxy_name;
			_init_value_type(itype);
			return itype;
		}

		static TypeInterface create_value_type(const StringName &p_cname, const String &p_proxy_name) {
			TypeInterface itype;
			itype.name = p_cname;
			itype.cname = p_cname;
			itype.proxy_name = p_proxy_name;
			_init_value_type(itype);
			return itype;
		}

		static TypeInterface create_value_type(const String &p_name) {
			TypeInterface itype;
			itype.name = p_name;
			itype.cname = p_name;
			_init_value_type(itype);
			return itype;
		}

		static TypeInterface create_value_type(const StringName &p_cname) {
			TypeInterface itype;
			itype.name = p_cname;
			itype.cname = p_cname;
			_init_value_type(itype);
			return itype;
		}

		static TypeInterface create_object_type(const StringName &p_cname, const String &p_proxy_name, ClassDB::APIType p_api_type) {
			TypeInterface itype;
			itype.name = p_cname;
			itype.cname = p_cname;
			itype.proxy_name = p_proxy_name;
			_init_object_type(itype, p_api_type);
			return itype;
		}

		static TypeInterface create_object_type(const StringName &p_cname, ClassDB::APIType p_api_type) {
			TypeInterface itype;
			itype.name = p_cname;
			itype.cname = p_cname;
			_init_object_type(itype, p_api_type);
			return itype;
		}

		static void postsetup_enum_type(TypeInterface &r_enum_itype);

		TypeInterface() {
			static String default_cs_variant_to_managed = "VariantUtils.ConvertTo<%1>(%0)";
			static String default_cs_managed_to_variant = "VariantUtils.CreateFrom<%1>(%0)";
			cs_variant_to_managed = default_cs_variant_to_managed;
			cs_managed_to_variant = default_cs_managed_to_variant;
		}
	};

	struct InternalCall {
		String name;
		String unique_sig; // Unique signature to avoid duplicates in containers
		bool editor_only = false;

		bool is_vararg = false;
		bool is_static = false;
		TypeReference return_type;
		List<TypeReference> argument_types;

		_FORCE_INLINE_ int get_arguments_count() const { return argument_types.size(); }

		InternalCall() {}

		InternalCall(ClassDB::APIType api_type, const String &p_name, const String &p_unique_sig = String()) {
			name = p_name;
			unique_sig = p_unique_sig;
			editor_only = api_type == ClassDB::API_EDITOR;
		}

		inline bool operator==(const InternalCall &p_a) const {
			return p_a.unique_sig == unique_sig;
		}
	};

	bool log_print_enabled = true;
	bool initialized = false;

	HashMap<StringName, TypeInterface> obj_types;

	HashMap<StringName, TypeInterface> builtin_types;
	HashMap<StringName, TypeInterface> enum_types;

	List<EnumInterface> global_enums;
	List<ConstantInterface> global_constants;

	List<InternalCall> method_icalls;
	/// Stores the unique internal calls from [method_icalls] that are assigned to each method.
	HashMap<const MethodInterface *, const InternalCall *> method_icalls_map;

	HashMap<StringName, List<StringName>> blacklisted_methods;
	HashSet<StringName> compat_singletons;

	void _initialize_blacklisted_methods();
	void _initialize_compat_singletons();

	struct NameCache {
		StringName type_void = StaticCString::create("void");
		StringName type_Variant = StaticCString::create("Variant");
		StringName type_VarArg = StaticCString::create("VarArg");
		StringName type_Object = StaticCString::create("Object");
		StringName type_RefCounted = StaticCString::create("RefCounted");
		StringName type_RID = StaticCString::create("RID");
		StringName type_Callable = StaticCString::create("Callable");
		StringName type_Signal = StaticCString::create("Signal");
		StringName type_String = StaticCString::create("String");
		StringName type_StringName = StaticCString::create("StringName");
		StringName type_NodePath = StaticCString::create("NodePath");
		StringName type_Array_generic = StaticCString::create("Array_@generic");
		StringName type_Dictionary_generic = StaticCString::create("Dictionary_@generic");
		StringName type_at_GlobalScope = StaticCString::create("@GlobalScope");
		StringName enum_Error = StaticCString::create("Error");

		StringName type_sbyte = StaticCString::create("sbyte");
		StringName type_short = StaticCString::create("short");
		StringName type_int = StaticCString::create("int");
		StringName type_byte = StaticCString::create("byte");
		StringName type_ushort = StaticCString::create("ushort");
		StringName type_uint = StaticCString::create("uint");
		StringName type_long = StaticCString::create("long");
		StringName type_ulong = StaticCString::create("ulong");

		StringName type_bool = StaticCString::create("bool");
		StringName type_float = StaticCString::create("float");
		StringName type_double = StaticCString::create("double");

		StringName type_Vector2 = StaticCString::create("Vector2");
		StringName type_Rect2 = StaticCString::create("Rect2");
		StringName type_Vector3 = StaticCString::create("Vector3");
		StringName type_Vector3i = StaticCString::create("Vector3i");
		StringName type_Vector4 = StaticCString::create("Vector4");
		StringName type_Vector4i = StaticCString::create("Vector4i");

		// Object not included as it must be checked for all derived classes
		static constexpr int nullable_types_count = 19;
		StringName nullable_types[nullable_types_count] = {
			type_String,
			type_StringName,
			type_NodePath,

			type_Array_generic,
			type_Dictionary_generic,
			StaticCString::create(_STR(Array)),
			StaticCString::create(_STR(Dictionary)),
			StaticCString::create(_STR(Callable)),
			StaticCString::create(_STR(Signal)),

			StaticCString::create(_STR(PackedByteArray)),
			StaticCString::create(_STR(PackedInt32Array)),
			StaticCString::create(_STR(PackedInt64Array)),
			StaticCString::create(_STR(PackedFloat32Array)),
			StaticCString::create(_STR(PackedFloat64Array)),
			StaticCString::create(_STR(PackedStringArray)),
			StaticCString::create(_STR(PackedVector2Array)),
			StaticCString::create(_STR(PackedVector3Array)),
			StaticCString::create(_STR(PackedColorArray)),
			StaticCString::create(_STR(PackedVector4Array)),
		};

		bool is_nullable_type(const StringName &p_type) const {
			for (int i = 0; i < nullable_types_count; i++) {
				if (p_type == nullable_types[i]) {
					return true;
				}
			}

			return false;
		}

		NameCache() {}

	private:
		NameCache(const NameCache &);
		void operator=(const NameCache &);
	};

	NameCache name_cache;

	const ConstantInterface *find_constant_by_name(const String &p_name, const List<ConstantInterface> &p_constants) const {
		for (const ConstantInterface &E : p_constants) {
			if (E.name == p_name) {
				return &E;
			}
		}

		return nullptr;
	}

	inline String get_arg_unique_sig(const TypeInterface &p_type) {
		// For parameters, we treat reference and non-reference derived types the same.
		if (p_type.is_object_type) {
			return "Obj";
		} else if (p_type.is_enum) {
			return "int";
		} else if (p_type.cname == name_cache.type_Array_generic) {
			return "Array";
		} else if (p_type.cname == name_cache.type_Dictionary_generic) {
			return "Dictionary";
		}

		return p_type.name;
	}

	inline String get_ret_unique_sig(const TypeInterface *p_type) {
		// Reference derived return types are treated differently.
		if (p_type->is_ref_counted) {
			return "Ref";
		} else if (p_type->is_object_type) {
			return "Obj";
		} else if (p_type->is_enum) {
			return "int";
		} else if (p_type->cname == name_cache.type_Array_generic) {
			return "Array";
		} else if (p_type->cname == name_cache.type_Dictionary_generic) {
			return "Dictionary";
		}

		return p_type->name;
	}

	String bbcode_to_text(const String &p_bbcode, const TypeInterface *p_itype);
	String bbcode_to_xml(const String &p_bbcode, const TypeInterface *p_itype, bool p_is_signal = false);

	void _append_text_method(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts);
	void _append_text_member(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts);
	void _append_text_signal(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts);
	void _append_text_enum(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts);
	void _append_text_constant(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts);
	void _append_text_constant_in_global_scope(StringBuilder &p_output, const String &p_target_cname, const String &p_link_target);
	void _append_text_param(StringBuilder &p_output, const String &p_link_target);
	void _append_text_undeclared(StringBuilder &p_output, const String &p_link_target);

	void _append_xml_method(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts, const TypeInterface *p_source_itype);
	void _append_xml_member(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts, const TypeInterface *p_source_itype);
	void _append_xml_signal(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts, const TypeInterface *p_source_itype);
	void _append_xml_enum(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts, const TypeInterface *p_source_itype);
	void _append_xml_constant(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts);
	void _append_xml_constant_in_global_scope(StringBuilder &p_xml_output, const String &p_target_cname, const String &p_link_target);
	void _append_xml_param(StringBuilder &p_xml_output, const String &p_link_target, bool p_is_signal);
	void _append_xml_undeclared(StringBuilder &p_xml_output, const String &p_link_target);

	bool _validate_api_type(const TypeInterface *p_target_itype, const TypeInterface *p_source_itype);

	int _determine_enum_prefix(const EnumInterface &p_ienum);
	void _apply_prefix_to_enum_constants(EnumInterface &p_ienum, int p_prefix_length);

	Error _populate_method_icalls_table(const TypeInterface &p_itype);

	const TypeInterface *_get_type_or_null(const TypeReference &p_typeref);
	const TypeInterface *_get_type_or_singleton_or_null(const TypeReference &p_typeref);

	const String _get_generic_type_parameters(const TypeInterface &p_itype, const List<TypeReference> &p_generic_type_parameters);

	StringName _get_type_name_from_meta(Variant::Type p_type, GodotTypeInfo::Metadata p_meta);
	StringName _get_int_type_name_from_meta(GodotTypeInfo::Metadata p_meta);
	StringName _get_float_type_name_from_meta(GodotTypeInfo::Metadata p_meta);

	bool _arg_default_value_from_variant(const Variant &p_val, ArgumentInterface &r_iarg);
	bool _arg_default_value_is_assignable_to_type(const Variant &p_val, const TypeInterface &p_arg_type);

	bool _populate_object_type_interfaces();
	void _populate_builtin_type_interfaces();

	void _populate_global_constants();

	bool _method_has_conflicting_signature(const MethodInterface &p_imethod, const TypeInterface &p_itype);
	bool _method_has_conflicting_signature(const MethodInterface &p_imethod_left, const MethodInterface &p_imethod_right);

	Error _generate_cs_type(const TypeInterface &itype, const String &p_output_file);

	Error _generate_cs_property(const TypeInterface &p_itype, const PropertyInterface &p_iprop, StringBuilder &p_output);
	Error _generate_cs_method(const TypeInterface &p_itype, const MethodInterface &p_imethod, int &p_method_bind_count, StringBuilder &p_output, bool p_use_span);
	Error _generate_cs_signal(const BindingsGenerator::TypeInterface &p_itype, const BindingsGenerator::SignalInterface &p_isignal, StringBuilder &p_output);

	Error _generate_cs_native_calls(const InternalCall &p_icall, StringBuilder &r_output);

	void _generate_array_extensions(StringBuilder &p_output);
	void _generate_global_constants(StringBuilder &p_output);

	Error _save_file(const String &p_path, const StringBuilder &p_content);

	void _log(const char *p_format, ...) _PRINTF_FORMAT_ATTRIBUTE_2_3;

	void _initialize();

public:
	Error generate_cs_core_project(const String &p_proj_dir);
	Error generate_cs_editor_project(const String &p_proj_dir);
	Error generate_cs_api(const String &p_output_dir);

	_FORCE_INLINE_ bool is_log_print_enabled() { return log_print_enabled; }
	_FORCE_INLINE_ void set_log_print_enabled(bool p_enabled) { log_print_enabled = p_enabled; }

	_FORCE_INLINE_ bool is_initialized() { return initialized; }

	static void handle_cmdline_args(const List<String> &p_cmdline_args);

	BindingsGenerator() {
		_initialize();
	}
};

#endif

#endif // BINDINGS_GENERATOR_H
