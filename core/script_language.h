/**************************************************************************/
/*  script_language.h                                                     */
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

#ifndef SCRIPT_LANGUAGE_H
#define SCRIPT_LANGUAGE_H

#include "core/io/multiplayer_api.h"
#include "core/map.h"
#include "core/pair.h"
#include "core/resource.h"

class ScriptLanguage;

typedef void (*ScriptEditRequestFunction)(const String &p_path);

class ScriptServer {
	enum {

		MAX_LANGUAGES = 16
	};

	static ScriptLanguage *_languages[MAX_LANGUAGES];
	static int _language_count;
	static bool scripting_enabled;
	static bool reload_scripts_on_save;
	static bool languages_finished;

	struct GlobalScriptClass {
		StringName language;
		String path;
		String base;
	};

	static HashMap<StringName, GlobalScriptClass> global_classes;

public:
	static ScriptEditRequestFunction edit_request_func;

	static void set_scripting_enabled(bool p_enabled);
	static bool is_scripting_enabled();
	_FORCE_INLINE_ static int get_language_count() { return _language_count; }
	static ScriptLanguage *get_language(int p_idx);
	static void register_language(ScriptLanguage *p_language);
	static void unregister_language(ScriptLanguage *p_language);

	static void set_reload_scripts_on_save(bool p_enable);
	static bool is_reload_scripts_on_save_enabled();

	static void thread_enter();
	static void thread_exit();

	static void global_classes_clear();
	static void add_global_class(const StringName &p_class, const StringName &p_base, const StringName &p_language, const String &p_path);
	static void remove_global_class(const StringName &p_class);
	static bool is_global_class(const StringName &p_class);
	static StringName get_global_class_language(const StringName &p_class);
	static String get_global_class_path(const String &p_class);
	static StringName get_global_class_base(const String &p_class);
	static StringName get_global_class_native_base(const String &p_class);
	static void get_global_class_list(List<StringName> *r_global_classes);
	static void save_global_classes();

	static void init_languages();
	static void finish_languages();

	static bool are_languages_finished() { return languages_finished; }
};

class ScriptInstance;
class PlaceHolderScriptInstance;

class Script : public Resource {
	GDCLASS(Script, Resource);
	OBJ_SAVE_TYPE(Script);

protected:
	virtual bool editor_can_reload_from_file() { return false; } // this is handled by editor better
	void _notification(int p_what);
	static void _bind_methods();

	friend class PlaceHolderScriptInstance;
	virtual void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {}

	Variant _get_property_default_value(const StringName &p_property);
	Array _get_script_property_list();
	Array _get_script_method_list();
	Array _get_script_signal_list();
	Dictionary _get_script_constant_map();

public:
	virtual bool can_instance() const = 0;

	virtual Ref<Script> get_base_script() const = 0; //for script inheritance

	virtual bool inherits_script(const Ref<Script> &p_script) const = 0;

	virtual StringName get_instance_base_type() const = 0; // this may not work in all scripts, will return empty if so
	virtual ScriptInstance *instance_create(Object *p_this) = 0;
	virtual PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) { return nullptr; }
	virtual bool instance_has(const Object *p_this) const = 0;

	virtual bool has_source_code() const = 0;
	virtual String get_source_code() const = 0;
	virtual void set_source_code(const String &p_code) = 0;
	virtual Error reload(bool p_keep_state = false) = 0;

	virtual bool has_method(const StringName &p_method) const = 0;
	virtual MethodInfo get_method_info(const StringName &p_method) const = 0;

	virtual bool is_tool() const = 0;
	virtual bool is_valid() const = 0;

	virtual ScriptLanguage *get_language() const = 0;

	virtual bool has_script_signal(const StringName &p_signal) const = 0;
	virtual void get_script_signal_list(List<MethodInfo> *r_signals) const = 0;

	virtual bool get_property_default_value(const StringName &p_property, Variant &r_value) const = 0;

	virtual void update_exports() {} //editor tool
	virtual void get_script_method_list(List<MethodInfo> *p_list) const = 0;
	virtual void get_script_property_list(List<PropertyInfo> *p_list) const = 0;

	virtual int get_member_line(const StringName &p_member) const { return -1; }

	virtual void get_constants(Map<StringName, Variant> *p_constants) {}
	virtual void get_members(Set<StringName> *p_constants) {}

	virtual bool is_placeholder_fallback_enabled() const { return false; }

	Script() {}
};

class ScriptInstance {
public:
	virtual bool set(const StringName &p_name, const Variant &p_value) = 0;
	virtual bool get(const StringName &p_name, Variant &r_ret) const = 0;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const = 0;
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const = 0;

	virtual Object *get_owner() { return nullptr; }
	virtual void get_property_state(List<Pair<StringName, Variant>> &state);

	virtual void get_method_list(List<MethodInfo> *p_list) const = 0;
	virtual bool has_method(const StringName &p_method) const = 0;
	virtual Variant call(const StringName &p_method, VARIANT_ARG_LIST);
	virtual Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) = 0;
	virtual void call_multilevel(const StringName &p_method, VARIANT_ARG_LIST);
	virtual void call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount);
	virtual void call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount);
	virtual void notification(int p_notification) = 0;
	virtual String to_string(bool *r_valid) {
		if (r_valid) {
			*r_valid = false;
		}
		return String();
	}

	//this is used by script languages that keep a reference counter of their own
	//you can make make Ref<> not die when it reaches zero, so deleting the reference
	//depends entirely from the script

	virtual void refcount_incremented() {}
	virtual bool refcount_decremented() { return true; } //return true if it can die

	virtual Ref<Script> get_script() const = 0;

	virtual bool is_placeholder() const { return false; }

	virtual void property_set_fallback(const StringName &p_name, const Variant &p_value, bool *r_valid);
	virtual Variant property_get_fallback(const StringName &p_name, bool *r_valid);

	virtual MultiplayerAPI::RPCMode get_rpc_mode(const StringName &p_method) const = 0;
	virtual MultiplayerAPI::RPCMode get_rset_mode(const StringName &p_variable) const = 0;

	virtual ScriptLanguage *get_language() = 0;
	virtual ~ScriptInstance();
};

struct ScriptCodeCompletionOption {
	enum Kind {
		KIND_CLASS,
		KIND_FUNCTION,
		KIND_SIGNAL,
		KIND_VARIABLE,
		KIND_MEMBER,
		KIND_ENUM,
		KIND_CONSTANT,
		KIND_NODE_PATH,
		KIND_FILE_PATH,
		KIND_PLAIN_TEXT,
	};
	Kind kind;
	String display;
	String insert_text;
	RES icon;
	Variant default_value;

	ScriptCodeCompletionOption() {
		kind = KIND_PLAIN_TEXT;
	}

	ScriptCodeCompletionOption(const String &p_text, Kind p_kind) {
		display = p_text;
		insert_text = p_text;
		kind = p_kind;
	}
};

class ScriptCodeCompletionCache {
	static ScriptCodeCompletionCache *singleton;

public:
	virtual RES get_cached_resource(const String &p_path) = 0;

	static ScriptCodeCompletionCache *get_singleton() { return singleton; }

	ScriptCodeCompletionCache();

	virtual ~ScriptCodeCompletionCache() {}
};

class ScriptLanguage {
public:
	virtual String get_name() const = 0;

	/* LANGUAGE FUNCTIONS */
	virtual void init() = 0;
	virtual String get_type() const = 0;
	virtual String get_extension() const = 0;
	virtual Error execute_file(const String &p_path) = 0;
	virtual void finish() = 0;

	/* EDITOR FUNCTIONS */
	struct Warning {
		int line;
		int code;
		String string_code;
		String message;
	};

	virtual void get_reserved_words(List<String> *p_words) const = 0;
	virtual bool is_control_flow_keyword(String p_string) const = 0;
	virtual void get_comment_delimiters(List<String> *p_delimiters) const = 0;
	virtual void get_string_delimiters(List<String> *p_delimiters) const = 0;
	virtual Ref<Script> get_template(const String &p_class_name, const String &p_base_class_name) const = 0;
	virtual void make_template(const String &p_class_name, const String &p_base_class_name, Ref<Script> &p_script) {}
	virtual bool is_using_templates() { return false; }
	virtual bool validate(const String &p_script, int &r_line_error, int &r_col_error, String &r_test_error, const String &p_path = "", List<String> *r_functions = nullptr, List<Warning> *r_warnings = nullptr, Set<int> *r_safe_lines = nullptr) const = 0;
	virtual String validate_path(const String &p_path) const { return ""; }
	virtual Script *create_script() const = 0;
	virtual bool has_named_classes() const = 0;
	virtual bool supports_builtin_mode() const = 0;
	virtual bool can_inherit_from_file() { return false; }
	virtual int find_function(const String &p_function, const String &p_code) const = 0;
	virtual String make_function(const String &p_class, const String &p_name, const PoolStringArray &p_args) const = 0;
	virtual Error open_in_external_editor(const Ref<Script> &p_script, int p_line, int p_col) { return ERR_UNAVAILABLE; }
	virtual bool overrides_external_editor() { return false; }

	virtual Error complete_code(const String &p_code, const String &p_path, Object *p_owner, List<ScriptCodeCompletionOption> *r_options, bool &r_force, String &r_call_hint) { return ERR_UNAVAILABLE; }

	struct LookupResult {
		enum Type {
			RESULT_SCRIPT_LOCATION,
			RESULT_CLASS,
			RESULT_CLASS_CONSTANT,
			RESULT_CLASS_PROPERTY,
			RESULT_CLASS_METHOD,
			RESULT_CLASS_ENUM,
			RESULT_CLASS_TBD_GLOBALSCOPE
		};
		Type type;
		Ref<Script> script;
		String class_name;
		String class_member;
		int location;
	};

	virtual Error lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner, LookupResult &r_result) { return ERR_UNAVAILABLE; }

	virtual void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const = 0;
	virtual void add_global_constant(const StringName &p_variable, const Variant &p_value) = 0;
	virtual void add_named_global_constant(const StringName &p_name, const Variant &p_value) {}
	virtual void remove_named_global_constant(const StringName &p_name) {}

	/* MULTITHREAD FUNCTIONS */

	//some VMs need to be notified of thread creation/exiting to allocate a stack
	virtual void thread_enter() {}
	virtual void thread_exit() {}

	/* DEBUGGER FUNCTIONS */

	virtual String debug_get_error() const = 0;
	virtual int debug_get_stack_level_count() const = 0;
	virtual int debug_get_stack_level_line(int p_level) const = 0;
	virtual String debug_get_stack_level_function(int p_level) const = 0;
	virtual String debug_get_stack_level_source(int p_level) const = 0;
	virtual void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) = 0;
	virtual void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) = 0;
	virtual ScriptInstance *debug_get_stack_level_instance(int p_level) { return nullptr; }
	virtual void debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) = 0;
	virtual String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems = -1, int p_max_depth = -1) = 0;

	struct StackInfo {
		String file;
		String func;
		int line;
	};

	virtual Vector<StackInfo> debug_get_current_stack_info() { return Vector<StackInfo>(); }

	virtual void reload_all_scripts() = 0;
	virtual void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) = 0;
	/* LOADER FUNCTIONS */

	virtual void get_recognized_extensions(List<String> *p_extensions) const = 0;
	virtual void get_public_functions(List<MethodInfo> *p_functions) const = 0;
	virtual void get_public_constants(List<Pair<String, Variant>> *p_constants) const = 0;

	struct ProfilingInfo {
		StringName signature;
		uint64_t call_count;
		uint64_t total_time;
		uint64_t self_time;
	};

	virtual void profiling_start() = 0;
	virtual void profiling_stop() = 0;

	virtual int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) = 0;
	virtual int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) = 0;

	virtual void *alloc_instance_binding_data(Object *p_object) { return nullptr; } //optional, not used by all languages
	virtual void free_instance_binding_data(Object *p_object, void *p_data) {} //optional, not used by all languages
	virtual void refcount_incremented_instance_binding(Object *p_object) {} //optional, not used by all languages
	virtual bool refcount_decremented_instance_binding(Object *p_object) { return true; } //return true if it can die //optional, not used by all languages

	virtual void frame();

	virtual bool handles_global_class_type(const String &p_type) const { return false; }
	virtual String get_global_class_name(const String &p_path, String *r_base_type = nullptr, String *r_icon_path = nullptr) const { return String(); }

	virtual ~ScriptLanguage() {}
};

extern uint8_t script_encryption_key[32];

class PlaceHolderScriptInstance : public ScriptInstance {
	Object *owner;
	List<PropertyInfo> properties;
	Map<StringName, Variant> values;
	Map<StringName, Variant> constants;
	ScriptLanguage *language;
	Ref<Script> script;

public:
	virtual bool set(const StringName &p_name, const Variant &p_value);
	virtual bool get(const StringName &p_name, Variant &r_ret) const;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const;
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const;

	virtual void get_method_list(List<MethodInfo> *p_list) const;
	virtual bool has_method(const StringName &p_method) const;
	virtual Variant call(const StringName &p_method, VARIANT_ARG_LIST) { return Variant(); }
	virtual Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}
	//virtual void call_multilevel(const StringName& p_method,VARIANT_ARG_LIST) { return Variant(); }
	//virtual void call_multilevel(const StringName& p_method,const Variant** p_args,int p_argcount,Variant::CallError &r_error) { return Variant(); }
	virtual void notification(int p_notification) {}

	virtual Ref<Script> get_script() const { return script; }

	virtual ScriptLanguage *get_language() { return language; }

	Object *get_owner() { return owner; }

	void update(const List<PropertyInfo> &p_properties, const Map<StringName, Variant> &p_values); //likely changed in editor

	virtual bool is_placeholder() const { return true; }

	virtual void property_set_fallback(const StringName &p_name, const Variant &p_value, bool *r_valid = nullptr);
	virtual Variant property_get_fallback(const StringName &p_name, bool *r_valid = nullptr);

	virtual MultiplayerAPI::RPCMode get_rpc_mode(const StringName &p_method) const { return MultiplayerAPI::RPC_MODE_DISABLED; }
	virtual MultiplayerAPI::RPCMode get_rset_mode(const StringName &p_variable) const { return MultiplayerAPI::RPC_MODE_DISABLED; }

	PlaceHolderScriptInstance(ScriptLanguage *p_language, Ref<Script> p_script, Object *p_owner);
	~PlaceHolderScriptInstance();
};

class ScriptDebugger {
	int lines_left;
	int depth;

	static ScriptDebugger *singleton;
	Map<int, Set<StringName>> breakpoints;

	ScriptLanguage *break_lang;

public:
	_FORCE_INLINE_ static ScriptDebugger *get_singleton() { return singleton; }
	void set_lines_left(int p_left);
	int get_lines_left() const;

	void set_depth(int p_depth);
	int get_depth() const;

	String breakpoint_find_source(const String &p_source) const;
	void insert_breakpoint(int p_line, const StringName &p_source);
	void remove_breakpoint(int p_line, const StringName &p_source);
	bool is_breakpoint(int p_line, const StringName &p_source) const;
	bool is_breakpoint_line(int p_line) const;
	void clear_breakpoints();
	const Map<int, Set<StringName>> &get_breakpoints() const { return breakpoints; }

	virtual void debug(ScriptLanguage *p_script, bool p_can_continue = true, bool p_is_error_breakpoint = false) = 0;
	virtual void idle_poll();
	virtual void line_poll();

	void set_break_language(ScriptLanguage *p_lang);
	ScriptLanguage *get_break_language() const;

	virtual void send_message(const String &p_message, const Array &p_args) = 0;
	virtual void send_error(const String &p_func, const String &p_file, int p_line, const String &p_err, const String &p_descr, ErrorHandlerType p_type, const Vector<ScriptLanguage::StackInfo> &p_stack_info) = 0;

	virtual bool is_remote() const { return false; }
	virtual void request_quit() {}

	virtual void set_multiplayer(Ref<MultiplayerAPI> p_multiplayer) {}

	virtual bool is_profiling() const = 0;
	virtual void add_profiling_frame_data(const StringName &p_name, const Array &p_data) = 0;
	virtual void profiling_start() = 0;
	virtual void profiling_end() = 0;
	virtual void profiling_set_frame_times(float p_frame_time, float p_process_time, float p_physics_time, float p_physics_frame_time) = 0;

	ScriptDebugger();
	virtual ~ScriptDebugger() { singleton = nullptr; }
};

#endif // SCRIPT_LANGUAGE_H
