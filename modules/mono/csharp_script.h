/**************************************************************************/
/*  csharp_script.h                                                       */
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

#ifndef CSHARP_SCRIPT_H
#define CSHARP_SCRIPT_H

#include "mono_gc_handle.h"
#include "mono_gd/gd_mono.h"

#include "core/doc_data.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/script_language.h"
#include "core/templates/self_list.h"

#ifdef TOOLS_ENABLED
#include "editor/plugins/editor_plugin.h"
#endif

class CSharpScript;
class CSharpInstance;
class CSharpLanguage;

template <typename TScriptInstance, typename TScriptLanguage>
TScriptInstance *cast_script_instance(ScriptInstance *p_inst) {
	return dynamic_cast<TScriptInstance *>(p_inst);
}

#define CAST_CSHARP_INSTANCE(m_inst) (cast_script_instance<CSharpInstance, CSharpLanguage>(m_inst))

class CSharpScript : public Script {
	GDCLASS(CSharpScript, Script);

	friend class CSharpInstance;
	friend class CSharpLanguage;

public:
	struct TypeInfo {
		/**
		 * Name of the C# class.
		 */
		String class_name;

		/**
		 * Name of the native class this script derives from.
		 */
		StringName native_base_name;

		/**
		 * Path to the icon that will be used for this class by the editor.
		 */
		String icon_path;

		/**
		 * Script is marked as tool and runs in the editor.
		 */
		bool is_tool = false;

		/**
		 * Script is marked as global class and will be registered in the editor.
		 * Registered classes can be created using certain editor dialogs and
		 * can be referenced by name from other languages that support the feature.
		 */
		bool is_global_class = false;

		/**
		 * Script is declared abstract.
		 */
		bool is_abstract = false;

		/**
		 * The C# type that corresponds to this script is a constructed generic type.
		 * E.g.: `Dictionary<int, string>`
		 */
		bool is_constructed_generic_type = false;

		/**
		 * The C# type that corresponds to this script is a generic type definition.
		 * E.g.: `Dictionary<,>`
		 */
		bool is_generic_type_definition = false;

		/**
		 * The C# type that corresponds to this script contains generic type parameters,
		 * regardless of whether the type parameters are bound or not.
		 */
		bool is_generic() const {
			return is_constructed_generic_type || is_generic_type_definition;
		}

		/**
		 * Check if the script can be instantiated.
		 * C# types can't be instantiated if they are abstract or contain generic
		 * type parameters, but a CSharpScript is still created for them.
		 */
		bool can_instantiate() const {
			return !is_abstract && !is_generic_type_definition;
		}
	};

private:
	/**
	 * Contains the C# type information for this script.
	 */
	TypeInfo type_info;

	/**
	 * Scripts are valid when the corresponding C# class is found and used
	 * to extract the script info using the [update_script_class_info] method.
	 */
	bool valid = false;
	/**
	 * Scripts extract info from the C# class in the reload methods but,
	 * if the reload is not invalidated, then the current extracted info
	 * is still valid and there's no need to reload again.
	 */
	bool reload_invalidated = false;

	/**
	 * Base script that this script derives from, or null if it derives from a
	 * native Godot class.
	 */
	Ref<CSharpScript> base_script;

	HashSet<Object *> instances;

#ifdef GD_MONO_HOT_RELOAD
	struct StateBackup {
		// TODO
		// Replace with buffer containing the serialized state of managed scripts.
		// Keep variant state backup to use only with script instance placeholders.
		List<Pair<StringName, Variant>> properties;
		Dictionary event_signals;
	};

	HashSet<ObjectID> pending_reload_instances;
	RBMap<ObjectID, StateBackup> pending_reload_state;

	bool was_tool_before_reload = false;
	HashSet<ObjectID> pending_replace_placeholders;
#endif

	/**
	 * Script source code.
	 */
	String source;

	SelfList<CSharpScript> script_list = this;

	Dictionary rpc_config;

	struct EventSignalInfo {
		StringName name; // MethodInfo stores a string...
		MethodInfo method_info;
	};

	struct CSharpMethodInfo {
		StringName name; // MethodInfo stores a string...
		MethodInfo method_info;
	};

	Vector<EventSignalInfo> event_signals;
	Vector<CSharpMethodInfo> methods;

#ifdef TOOLS_ENABLED
	List<PropertyInfo> exported_members_cache; // members_cache
	HashMap<StringName, Variant> exported_members_defval_cache; // member_default_values_cache
	HashSet<PlaceHolderScriptInstance *> placeholders;
	bool source_changed_cache = false;
	bool placeholder_fallback_enabled = false;
	bool exports_invalidated = true;
	void _update_exports_values(HashMap<StringName, Variant> &values, List<PropertyInfo> &propnames);
	void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder) override;
#endif

#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
	HashSet<StringName> exported_members_names;
#endif

	HashMap<StringName, PropertyInfo> member_info;

	void _clear();

	static void GD_CLR_STDCALL _add_property_info_list_callback(CSharpScript *p_script, const String *p_current_class_name, void *p_props, int32_t p_count);
#ifdef TOOLS_ENABLED
	static void GD_CLR_STDCALL _add_property_default_values_callback(CSharpScript *p_script, void *p_def_vals, int32_t p_count);
#endif
	bool _update_exports(PlaceHolderScriptInstance *p_instance_to_update = nullptr);

	CSharpInstance *_create_instance(const Variant **p_args, int p_argcount, Object *p_owner, bool p_is_ref_counted, Callable::CallError &r_error);
	Variant _new(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	// Do not use unless you know what you are doing
	static void update_script_class_info(Ref<CSharpScript> p_script);

	void _get_script_signal_list(List<MethodInfo> *r_signals, bool p_include_base) const;

protected:
	static void _bind_methods();

	bool _get(const StringName &p_name, Variant &r_ret) const;
	bool _set(const StringName &p_name, const Variant &p_value);
	void _get_property_list(List<PropertyInfo> *p_properties) const;

public:
	static void reload_registered_script(Ref<CSharpScript> p_script);

	bool can_instantiate() const override;
	StringName get_instance_base_type() const override;
	ScriptInstance *instance_create(Object *p_this) override;
	PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override;
	bool instance_has(const Object *p_this) const override;

	bool has_source_code() const override;
	String get_source_code() const override;
	void set_source_code(const String &p_code) override;

#ifdef TOOLS_ENABLED
	virtual StringName get_doc_class_name() const override { return StringName(); } // TODO
	virtual Vector<DocData::ClassDoc> get_documentation() const override {
		// TODO
		Vector<DocData::ClassDoc> docs;
		return docs;
	}
	virtual String get_class_icon_path() const override {
		return type_info.icon_path;
	}
#endif // TOOLS_ENABLED

	Error reload(bool p_keep_state = false) override;

	bool has_script_signal(const StringName &p_signal) const override;
	void get_script_signal_list(List<MethodInfo> *r_signals) const override;

	bool get_property_default_value(const StringName &p_property, Variant &r_value) const override;
	void get_script_property_list(List<PropertyInfo> *r_list) const override;
	void update_exports() override;

	void get_members(HashSet<StringName> *p_members) override;

	bool is_tool() const override {
		return type_info.is_tool;
	}
	bool is_valid() const override {
		return valid;
	}
	bool is_abstract() const override {
		return type_info.is_abstract;
	}

	bool inherits_script(const Ref<Script> &p_script) const override;

	Ref<Script> get_base_script() const override;
	StringName get_global_name() const override;

	ScriptLanguage *get_language() const override;

	void get_script_method_list(List<MethodInfo> *p_list) const override;
	bool has_method(const StringName &p_method) const override;
	virtual int get_script_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override;
	MethodInfo get_method_info(const StringName &p_method) const override;
	Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;

	int get_member_line(const StringName &p_member) const override;

	Variant get_rpc_config() const override;

#ifdef TOOLS_ENABLED
	bool is_placeholder_fallback_enabled() const override {
		return placeholder_fallback_enabled;
	}
#endif

	Error load_source_code(const String &p_path);

	CSharpScript();
	~CSharpScript();
};

class CSharpInstance : public ScriptInstance {
	friend class CSharpScript;
	friend class CSharpLanguage;

	Object *owner = nullptr;
	bool base_ref_counted = false;
	bool ref_dying = false;
	bool unsafe_referenced = false;
	bool predelete_notified = false;
	bool destructing_script_instance = false;

	Ref<CSharpScript> script;
	MonoGCHandleData gchandle;

	List<Callable> connected_event_signals;

	bool _reference_owner_unsafe();

	/*
	 * If true is returned, the caller must memdelete the script instance's owner.
	 */
	bool _unreference_owner_unsafe();

	/*
	 * If false is returned, the caller must destroy the script instance by removing it from its owner.
	 */
	bool _internal_new_managed();

	// Do not use unless you know what you are doing
	static CSharpInstance *create_for_managed_type(Object *p_owner, CSharpScript *p_script, const MonoGCHandleData &p_gchandle);

public:
	_FORCE_INLINE_ bool is_destructing_script_instance() { return destructing_script_instance; }

	_FORCE_INLINE_ GCHandleIntPtr get_gchandle_intptr() { return gchandle.get_intptr(); }

	Object *get_owner() override;

	bool set(const StringName &p_name, const Variant &p_value) override;
	bool get(const StringName &p_name, Variant &r_ret) const override;
	void get_property_list(List<PropertyInfo> *p_properties) const override;
	Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid) const override;
	virtual void validate_property(PropertyInfo &p_property) const override;

	bool property_can_revert(const StringName &p_name) const override;
	bool property_get_revert(const StringName &p_name, Variant &r_ret) const override;

	void get_method_list(List<MethodInfo> *p_list) const override;
	bool has_method(const StringName &p_method) const override;
	virtual int get_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override;
	Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;

	void mono_object_disposed(GCHandleIntPtr p_gchandle_to_free);

	/*
	 * If 'r_delete_owner' is set to true, the caller must memdelete the script instance's owner. Otherwise, if
	 * 'r_remove_script_instance' is set to true, the caller must destroy the script instance by removing it from its owner.
	 */
	void mono_object_disposed_baseref(GCHandleIntPtr p_gchandle_to_free, bool p_is_finalizer, bool &r_delete_owner, bool &r_remove_script_instance);

	void connect_event_signals();
	void disconnect_event_signals();

	void refcount_incremented() override;
	bool refcount_decremented() override;

	const Variant get_rpc_config() const override;

	void notification(int p_notification, bool p_reversed = false) override;
	void _call_notification(int p_notification, bool p_reversed = false);

	String to_string(bool *r_valid) override;

	Ref<Script> get_script() const override;

	ScriptLanguage *get_language() override;

	CSharpInstance(const Ref<CSharpScript> &p_script);
	~CSharpInstance();
};

struct CSharpScriptBinding {
	bool inited = false;
	StringName type_name;
	MonoGCHandleData gchandle;
	Object *owner = nullptr;

	CSharpScriptBinding() {}
};

class ManagedCallableMiddleman : public Object {
	GDCLASS(ManagedCallableMiddleman, Object);
};

class CSharpLanguage : public ScriptLanguage {
	friend class CSharpScript;
	friend class CSharpInstance;

	static CSharpLanguage *singleton;

	bool finalizing = false;
	bool finalized = false;

	GDMono *gdmono = nullptr;
	SelfList<CSharpScript>::List script_list;

	Mutex script_instances_mutex;
	Mutex script_gchandle_release_mutex;
	Mutex language_bind_mutex;

	RBMap<Object *, CSharpScriptBinding> script_bindings;

#ifdef DEBUG_ENABLED
	// List of unsafe object references
	HashMap<ObjectID, int> unsafe_object_references;
	Mutex unsafe_object_references_lock;
#endif

	ManagedCallableMiddleman *managed_callable_middleman = memnew(ManagedCallableMiddleman);

	int lang_idx = -1;

	// For debug_break and debug_break_parse
	int _debug_parse_err_line = -1;
	String _debug_parse_err_file;
	String _debug_error;

	friend class GDMono;

#ifdef TOOLS_ENABLED
	EditorPlugin *godotsharp_editor = nullptr;

	static void _editor_init_callback();
#endif

	static void *_instance_binding_create_callback(void *p_token, void *p_instance);
	static void _instance_binding_free_callback(void *p_token, void *p_instance, void *p_binding);
	static GDExtensionBool _instance_binding_reference_callback(void *p_token, void *p_binding, GDExtensionBool p_reference);

	static GDExtensionInstanceBindingCallbacks _instance_binding_callbacks;

public:
	static void *get_instance_binding(Object *p_object);
	static void *get_existing_instance_binding(Object *p_object);
	static void *get_instance_binding_with_setup(Object *p_object);
	static bool has_instance_binding(Object *p_object);

	const Mutex &get_language_bind_mutex() {
		return language_bind_mutex;
	}
	const Mutex &get_script_instances_mutex() {
		return script_instances_mutex;
	}

	_FORCE_INLINE_ int get_language_index() {
		return lang_idx;
	}
	void set_language_index(int p_idx);

	_FORCE_INLINE_ static CSharpLanguage *get_singleton() {
		return singleton;
	}

#ifdef TOOLS_ENABLED
	_FORCE_INLINE_ EditorPlugin *get_godotsharp_editor() const {
		return godotsharp_editor;
	}
#endif

	static void release_script_gchandle(MonoGCHandleData &p_gchandle);
	static void release_script_gchandle_thread_safe(GCHandleIntPtr p_gchandle_to_free, MonoGCHandleData &r_gchandle);
	static void release_binding_gchandle_thread_safe(GCHandleIntPtr p_gchandle_to_free, CSharpScriptBinding &r_script_binding);

	bool debug_break(const String &p_error, bool p_allow_continue = true);
	bool debug_break_parse(const String &p_file, int p_line, const String &p_error);

#ifdef GD_MONO_HOT_RELOAD
	bool is_assembly_reloading_needed();
	void reload_assemblies(bool p_soft_reload);
#endif

	_FORCE_INLINE_ ManagedCallableMiddleman *get_managed_callable_middleman() const {
		return managed_callable_middleman;
	}

	String get_name() const override;

	/* LANGUAGE FUNCTIONS */
	String get_type() const override;
	String get_extension() const override;
	void init() override;
	void finish() override;

	void finalize();

	/* EDITOR FUNCTIONS */
	void get_reserved_words(List<String> *p_words) const override;
	bool is_control_flow_keyword(const String &p_keyword) const override;
	void get_comment_delimiters(List<String> *p_delimiters) const override;
	void get_doc_comment_delimiters(List<String> *p_delimiters) const override;
	void get_string_delimiters(List<String> *p_delimiters) const override;
	bool is_using_templates() override;
	virtual Ref<Script> make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const override;
	virtual Vector<ScriptTemplate> get_built_in_templates(const StringName &p_object) override;
	/* TODO */ bool validate(const String &p_script, const String &p_path, List<String> *r_functions,
			List<ScriptLanguage::ScriptError> *r_errors = nullptr, List<ScriptLanguage::Warning> *r_warnings = nullptr, HashSet<int> *r_safe_lines = nullptr) const override {
		return true;
	}
	String validate_path(const String &p_path) const override;
	Script *create_script() const override;
#ifndef DISABLE_DEPRECATED
	virtual bool has_named_classes() const override { return false; }
#endif
	bool supports_builtin_mode() const override;
	/* TODO? */ int find_function(const String &p_function, const String &p_code) const override {
		return -1;
	}
	String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const override;
	virtual bool can_make_function() const override { return false; }
	virtual String _get_indentation() const;
	/* TODO? */ void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const override {}
	/* TODO */ void add_global_constant(const StringName &p_variable, const Variant &p_value) override {}
	virtual ScriptNameCasing preferred_file_name_casing() const override;

	/* SCRIPT GLOBAL CLASS FUNCTIONS */
	virtual bool handles_global_class_type(const String &p_type) const override;
	virtual String get_global_class_name(const String &p_path, String *r_base_type = nullptr, String *r_icon_path = nullptr) const override;

	/* DEBUGGER FUNCTIONS */
	String debug_get_error() const override;
	int debug_get_stack_level_count() const override;
	int debug_get_stack_level_line(int p_level) const override;
	String debug_get_stack_level_function(int p_level) const override;
	String debug_get_stack_level_source(int p_level) const override;
	/* TODO */ void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) override {}
	/* TODO */ void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) override {}
	/* TODO */ void debug_get_globals(List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) override {}
	/* TODO */ String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) override {
		return "";
	}
	Vector<StackInfo> debug_get_current_stack_info() override;

	/* PROFILING FUNCTIONS */
	/* TODO */ void profiling_start() override {}
	/* TODO */ void profiling_stop() override {}
	/* TODO */ void profiling_set_save_native_calls(bool p_enable) override {}
	/* TODO */ int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) override {
		return 0;
	}
	/* TODO */ int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) override {
		return 0;
	}

	void frame() override;

	/* TODO? */ void get_public_functions(List<MethodInfo> *p_functions) const override {}
	/* TODO? */ void get_public_constants(List<Pair<String, Variant>> *p_constants) const override {}
	/* TODO? */ void get_public_annotations(List<MethodInfo> *p_annotations) const override {}

	void reload_all_scripts() override;
	void reload_scripts(const Array &p_scripts, bool p_soft_reload) override;
	void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) override;

	/* LOADER FUNCTIONS */
	void get_recognized_extensions(List<String> *p_extensions) const override;

#ifdef TOOLS_ENABLED
	Error open_in_external_editor(const Ref<Script> &p_script, int p_line, int p_col) override;
	bool overrides_external_editor() override;
#endif

	RBMap<Object *, CSharpScriptBinding>::Element *insert_script_binding(Object *p_object, const CSharpScriptBinding &p_script_binding);
	bool setup_csharp_script_binding(CSharpScriptBinding &r_script_binding, Object *p_object);

	static void tie_native_managed_to_unmanaged(GCHandleIntPtr p_gchandle_intptr, Object *p_unmanaged, const StringName *p_native_name, bool p_ref_counted);
	static void tie_user_managed_to_unmanaged(GCHandleIntPtr p_gchandle_intptr, Object *p_unmanaged, Ref<CSharpScript> *p_script, bool p_ref_counted);
	static void tie_managed_to_unmanaged_with_pre_setup(GCHandleIntPtr p_gchandle_intptr, Object *p_unmanaged);

	void post_unsafe_reference(Object *p_obj);
	void pre_unsafe_unreference(Object *p_obj);

	CSharpLanguage();
	~CSharpLanguage();
};

class ResourceFormatLoaderCSharpScript : public ResourceFormatLoader {
public:
	Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	void get_recognized_extensions(List<String> *p_extensions) const override;
	bool handles_type(const String &p_type) const override;
	String get_resource_type(const String &p_path) const override;
};

class ResourceFormatSaverCSharpScript : public ResourceFormatSaver {
public:
	Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	bool recognize(const Ref<Resource> &p_resource) const override;
};

#endif // CSHARP_SCRIPT_H
