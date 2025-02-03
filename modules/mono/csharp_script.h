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

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/script_language.h"
#include "core/self_list.h"

#include "mono_gc_handle.h"
#include "mono_gd/gd_mono.h"
#include "mono_gd/gd_mono_header.h"
#include "mono_gd/gd_mono_internals.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_plugin.h"
#endif

class CSharpScript;
class CSharpInstance;
class CSharpLanguage;

#ifdef NO_SAFE_CAST
template <typename TScriptInstance, typename TScriptLanguage>
TScriptInstance *cast_script_instance(ScriptInstance *p_inst) {
	if (!p_inst)
		return NULL;
	return p_inst->get_language() == TScriptLanguage::get_singleton() ? static_cast<TScriptInstance *>(p_inst) : NULL;
}
#else
template <typename TScriptInstance, typename TScriptLanguage>
TScriptInstance *cast_script_instance(ScriptInstance *p_inst) {
	return dynamic_cast<TScriptInstance *>(p_inst);
}
#endif

#define CAST_CSHARP_INSTANCE(m_inst) (cast_script_instance<CSharpInstance, CSharpLanguage>(m_inst))

class CSharpScript : public Script {
	GDCLASS(CSharpScript, Script);

	friend class CSharpInstance;
	friend class CSharpLanguage;
	friend struct CSharpScriptDepSort;

	bool tool;
	bool valid;
	bool reload_invalidated;

	bool builtin;

	GDMonoClass *base;
	GDMonoClass *native;
	GDMonoClass *script_class;

	Ref<CSharpScript> base_cache; // TODO what's this for?

	Set<Object *> instances;

#ifdef GD_MONO_HOT_RELOAD
	struct StateBackup {
		// TODO
		// Replace with buffer containing the serialized state of managed scripts.
		// Keep variant state backup to use only with script instance placeholders.
		List<Pair<StringName, Variant>> properties;
	};

	Set<ObjectID> pending_reload_instances;
	Map<ObjectID, StateBackup> pending_reload_state;
	StringName tied_class_name_for_reload;
	StringName tied_class_namespace_for_reload;
#endif

	String source;
	StringName name;

	SelfList<CSharpScript> script_list;

	struct Argument {
		String name;
		Variant::Type type;
	};

	Map<StringName, Vector<Argument>> _signals;
	bool signals_invalidated;

#ifdef TOOLS_ENABLED
	List<PropertyInfo> exported_members_cache; // members_cache
	Map<StringName, Variant> exported_members_defval_cache; // member_default_values_cache
	Set<PlaceHolderScriptInstance *> placeholders;
	bool source_changed_cache;
	bool placeholder_fallback_enabled;
	bool exports_invalidated;
	void _update_exports_values(Map<StringName, Variant> &values, List<PropertyInfo> &propnames);
	void _update_member_info_no_exports();
	virtual void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder);
#endif

#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
	Set<StringName> exported_members_names;
#endif

	OrderedHashMap<StringName, PropertyInfo> member_info;

	void _clear();

	void load_script_signals(GDMonoClass *p_class, GDMonoClass *p_native_class);
	bool _get_signal(GDMonoClass *p_class, GDMonoClass *p_delegate, Vector<Argument> &params);

	bool _update_exports(PlaceHolderScriptInstance *p_instance_to_update = nullptr);

	bool _get_member_export(IMonoClassMember *p_member, bool p_inspect_export, PropertyInfo &r_prop_info, bool &r_exported);
#ifdef TOOLS_ENABLED
	static int _try_get_member_export_hint(IMonoClassMember *p_member, ManagedType p_type, Variant::Type p_variant_type, bool p_allow_generics, PropertyHint &r_hint, String &r_hint_string);
#endif

	CSharpInstance *_create_instance(const Variant **p_args, int p_argcount, Object *p_owner, bool p_isref, Variant::CallError &r_error);
	Variant _new(const Variant **p_args, int p_argcount, Variant::CallError &r_error);

	// Do not use unless you know what you are doing
	friend void GDMonoInternals::tie_managed_to_unmanaged(MonoObject *, Object *);
	static Ref<CSharpScript> create_for_managed_type(GDMonoClass *p_class, GDMonoClass *p_native);
	static void initialize_for_managed_type(Ref<CSharpScript> p_script, GDMonoClass *p_class, GDMonoClass *p_native);

protected:
	static void _bind_methods();

	Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error);
	virtual void _resource_path_changed();
	bool _get(const StringName &p_name, Variant &r_ret) const;
	bool _set(const StringName &p_name, const Variant &p_value);
	void _get_property_list(List<PropertyInfo> *p_properties) const;

public:
	virtual bool can_instance() const;
	virtual StringName get_instance_base_type() const;
	virtual ScriptInstance *instance_create(Object *p_this);
	virtual PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this);
	virtual bool instance_has(const Object *p_this) const;

	virtual bool has_source_code() const;
	virtual String get_source_code() const;
	virtual void set_source_code(const String &p_code);

	virtual Error reload(bool p_keep_state = false);

	virtual bool has_script_signal(const StringName &p_signal) const;
	virtual void get_script_signal_list(List<MethodInfo> *r_signals) const;

	virtual bool get_property_default_value(const StringName &p_property, Variant &r_value) const;
	virtual void get_script_property_list(List<PropertyInfo> *p_list) const;
	virtual void update_exports();

	virtual void get_members(Set<StringName> *p_members);

	virtual bool is_tool() const { return tool; }
	virtual bool is_valid() const { return valid; }

	bool inherits_script(const Ref<Script> &p_script) const;

	virtual Ref<Script> get_base_script() const;
	virtual ScriptLanguage *get_language() const;

	virtual void get_script_method_list(List<MethodInfo> *p_list) const;
	bool has_method(const StringName &p_method) const;
	MethodInfo get_method_info(const StringName &p_method) const;

	virtual int get_member_line(const StringName &p_member) const;

#ifdef TOOLS_ENABLED
	virtual bool is_placeholder_fallback_enabled() const { return placeholder_fallback_enabled; }
#endif

	Error load_source_code(const String &p_path);

	StringName get_script_name() const;

	CSharpScript();
	~CSharpScript();
};

class CSharpInstance : public ScriptInstance {
	friend class CSharpScript;
	friend class CSharpLanguage;

	Object *owner;
	bool base_ref;
	bool ref_dying;
	bool unsafe_referenced;
	bool predelete_notified;
	bool destructing_script_instance;

	Ref<CSharpScript> script;
	Ref<MonoGCHandle> gchandle;

	bool _reference_owner_unsafe();

	/*
	 * If true is returned, the caller must memdelete the script instance's owner.
	 */
	bool _unreference_owner_unsafe();

	/*
	 * If NULL is returned, the caller must destroy the script instance by removing it from its owner.
	 */
	MonoObject *_internal_new_managed();

	// Do not use unless you know what you are doing
	friend void GDMonoInternals::tie_managed_to_unmanaged(MonoObject *, Object *);
	static CSharpInstance *create_for_managed_type(Object *p_owner, CSharpScript *p_script, const Ref<MonoGCHandle> &p_gchandle);

	void _call_multilevel(MonoObject *p_mono_object, const StringName &p_method, const Variant **p_args, int p_argcount);

	MultiplayerAPI::RPCMode _member_get_rpc_mode(IMonoClassMember *p_member) const;

	void get_properties_state_for_reloading(List<Pair<StringName, Variant>> &r_state);

public:
	MonoObject *get_mono_object() const;

	_FORCE_INLINE_ bool is_destructing_script_instance() { return destructing_script_instance; }

	virtual Object *get_owner();

	virtual bool set(const StringName &p_name, const Variant &p_value);
	virtual bool get(const StringName &p_name, Variant &r_ret) const;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const;
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid) const;

	virtual void get_method_list(List<MethodInfo> *p_list) const;
	virtual bool has_method(const StringName &p_method) const;
	virtual Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error);
	virtual void call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount);
	virtual void call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount);

	void mono_object_disposed(MonoObject *p_obj);

	/*
	 * If 'r_delete_owner' is set to true, the caller must memdelete the script instance's owner. Otherwise, if
	 * 'r_remove_script_instance' is set to true, the caller must destroy the script instance by removing it from its owner.
	 */
	void mono_object_disposed_baseref(MonoObject *p_obj, bool p_is_finalizer, bool &r_delete_owner, bool &r_remove_script_instance);

	virtual void refcount_incremented();
	virtual bool refcount_decremented();

	virtual MultiplayerAPI::RPCMode get_rpc_mode(const StringName &p_method) const;
	virtual MultiplayerAPI::RPCMode get_rset_mode(const StringName &p_variable) const;

	virtual void notification(int p_notification);
	void _call_notification(int p_notification);

	virtual String to_string(bool *r_valid);

	virtual Ref<Script> get_script() const;

	virtual ScriptLanguage *get_language();

	CSharpInstance();
	~CSharpInstance();
};

struct CSharpScriptBinding {
	bool inited;
	StringName type_name;
	GDMonoClass *wrapper_class;
	Ref<MonoGCHandle> gchandle;
	Object *owner;
};

class CSharpLanguage : public ScriptLanguage {
	friend class CSharpScript;
	friend class CSharpInstance;

	static CSharpLanguage *singleton;

	bool finalizing;

	GDMono *gdmono;
	SelfList<CSharpScript>::List script_list;

	Mutex script_instances_mutex;
	Mutex script_gchandle_release_mutex;
	Mutex language_bind_mutex;

	Map<Object *, CSharpScriptBinding> script_bindings;

#ifdef DEBUG_ENABLED
	// List of unsafe object references
	Map<ObjectID, int> unsafe_object_references;
	Mutex unsafe_object_references_lock;
#endif

	struct StringNameCache {
		StringName _signal_callback;
		StringName _set;
		StringName _get;
		StringName _get_property_list;
		StringName _notification;
		StringName _script_source;
		StringName dotctor; // .ctor
		StringName on_before_serialize; // OnBeforeSerialize
		StringName on_after_deserialize; // OnAfterDeserialize

		StringNameCache();
	};

	int lang_idx;

	Dictionary scripts_metadata;
	bool scripts_metadata_invalidated;

	// For debug_break and debug_break_parse
	int _debug_parse_err_line;
	String _debug_parse_err_file;
	String _debug_error;

	void _load_scripts_metadata();

	friend class GDMono;
	void _on_scripts_domain_unloaded();

#ifdef TOOLS_ENABLED
	EditorPlugin *godotsharp_editor;

	static void _editor_init_callback();
#endif

public:
	StringNameCache string_names;

	Mutex &get_language_bind_mutex() { return language_bind_mutex; }

	_FORCE_INLINE_ int get_language_index() { return lang_idx; }
	void set_language_index(int p_idx);

	_FORCE_INLINE_ const StringNameCache &get_string_names() { return string_names; }

	_FORCE_INLINE_ static CSharpLanguage *get_singleton() { return singleton; }

#ifdef TOOLS_ENABLED
	_FORCE_INLINE_ EditorPlugin *get_godotsharp_editor() const { return godotsharp_editor; }
#endif

	static void release_script_gchandle(Ref<MonoGCHandle> &p_gchandle);
	static void release_script_gchandle(MonoObject *p_expected_obj, Ref<MonoGCHandle> &p_gchandle);

	bool debug_break(const String &p_error, bool p_allow_continue = true);
	bool debug_break_parse(const String &p_file, int p_line, const String &p_error);

#ifdef GD_MONO_HOT_RELOAD
	bool is_assembly_reloading_needed();
	void reload_assemblies(bool p_soft_reload);
#endif

	_FORCE_INLINE_ Dictionary get_scripts_metadata_or_nothing() {
		return scripts_metadata_invalidated ? Dictionary() : scripts_metadata;
	}

	_FORCE_INLINE_ const Dictionary &get_scripts_metadata() {
		if (scripts_metadata_invalidated)
			_load_scripts_metadata();
		return scripts_metadata;
	}

	virtual String get_name() const;

	/* LANGUAGE FUNCTIONS */
	virtual String get_type() const;
	virtual String get_extension() const;
	virtual Error execute_file(const String &p_path);
	virtual void init();
	virtual void finish();

	/* EDITOR FUNCTIONS */
	virtual void get_reserved_words(List<String> *p_words) const;
	virtual bool is_control_flow_keyword(String p_keyword) const;
	virtual void get_comment_delimiters(List<String> *p_delimiters) const;
	virtual void get_string_delimiters(List<String> *p_delimiters) const;
	virtual Ref<Script> get_template(const String &p_class_name, const String &p_base_class_name) const;
	virtual bool is_using_templates();
	virtual void make_template(const String &p_class_name, const String &p_base_class_name, Ref<Script> &p_script);
	/* TODO */ virtual bool validate(const String &p_script, int &r_line_error, int &r_col_error, String &r_test_error, const String &p_path, List<String> *r_functions, List<ScriptLanguage::Warning> *r_warnings = NULL, Set<int> *r_safe_lines = NULL) const { return true; }
	virtual String validate_path(const String &p_path) const;
	virtual Script *create_script() const;
	virtual bool has_named_classes() const;
	virtual bool supports_builtin_mode() const;
	/* TODO? */ virtual int find_function(const String &p_function, const String &p_code) const { return -1; }
	virtual String make_function(const String &p_class, const String &p_name, const PoolStringArray &p_args) const;
	virtual String _get_indentation() const;
	/* TODO? */ virtual void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const {}
	/* TODO */ virtual void add_global_constant(const StringName &p_variable, const Variant &p_value) {}

	/* DEBUGGER FUNCTIONS */
	virtual String debug_get_error() const;
	virtual int debug_get_stack_level_count() const;
	virtual int debug_get_stack_level_line(int p_level) const;
	virtual String debug_get_stack_level_function(int p_level) const;
	virtual String debug_get_stack_level_source(int p_level) const;
	/* TODO */ virtual void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {}
	/* TODO */ virtual void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {}
	/* TODO */ virtual void debug_get_globals(List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {}
	/* TODO */ virtual String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) { return ""; }
	virtual Vector<StackInfo> debug_get_current_stack_info();

	/* PROFILING FUNCTIONS */
	/* TODO */ virtual void profiling_start() {}
	/* TODO */ virtual void profiling_stop() {}
	/* TODO */ virtual int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) { return 0; }
	/* TODO */ virtual int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) { return 0; }

	virtual void frame();

	/* TODO? */ virtual void get_public_functions(List<MethodInfo> *p_functions) const {}
	/* TODO? */ virtual void get_public_constants(List<Pair<String, Variant>> *p_constants) const {}

	virtual void reload_all_scripts();
	virtual void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload);

	/* LOADER FUNCTIONS */
	virtual void get_recognized_extensions(List<String> *p_extensions) const;

#ifdef TOOLS_ENABLED
	virtual Error open_in_external_editor(const Ref<Script> &p_script, int p_line, int p_col);
	virtual bool overrides_external_editor();
#endif

	/* THREAD ATTACHING */
	virtual void thread_enter();
	virtual void thread_exit();

	// Don't use these. I'm watching you
	virtual void *alloc_instance_binding_data(Object *p_object);
	virtual void free_instance_binding_data(Object *p_object, void *p_data);
	virtual void refcount_incremented_instance_binding(Object *p_object);
	virtual bool refcount_decremented_instance_binding(Object *p_object);

	Map<Object *, CSharpScriptBinding>::Element *insert_script_binding(Object *p_object, const CSharpScriptBinding &p_script_binding);
	bool setup_csharp_script_binding(CSharpScriptBinding &r_script_binding, Object *p_object);

#ifdef DEBUG_ENABLED
	Vector<StackInfo> stack_trace_get_info(MonoObject *p_stack_trace);
#endif

	void post_unsafe_reference(Object *p_obj);
	void pre_unsafe_unreference(Object *p_obj);

	CSharpLanguage();
	~CSharpLanguage();
};

class ResourceFormatLoaderCSharpScript : public ResourceFormatLoader {
public:
	virtual RES load(const String &p_path, const String &p_original_path = "", Error *r_error = NULL, bool p_no_subresource_cache = false);
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual bool handles_type(const String &p_type) const;
	virtual String get_resource_type(const String &p_path) const;
};

class ResourceFormatSaverCSharpScript : public ResourceFormatSaver {
public:
	virtual Error save(const String &p_path, const RES &p_resource, uint32_t p_flags = 0);
	virtual void get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const;
	virtual bool recognize(const RES &p_resource) const;
};

#endif // CSHARP_SCRIPT_H
