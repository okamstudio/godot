/**************************************************************************/
/*  csharp_script.cpp                                                     */
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

#include "csharp_script.h"

#include "godotsharp_dirs.h"
#include "managed_callable.h"
#include "mono_gd/gd_mono_cache.h"
#include "signal_awaiter_utils.h"
#include "utils/macros.h"
#include "utils/naming_utils.h"
#include "utils/path_utils.h"
#include "utils/string_utils.h"

#ifdef DEBUG_METHODS_ENABLED
#include "class_db_api_json.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_internal_calls.h"
#include "editor/script_templates/templates.gen.h"
#endif

#include "core/config/project_settings.h"
#include "core/debugger/engine_debugger.h"
#include "core/debugger/script_debugger.h"
#include "core/io/file_access.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/text_server.h"

#ifdef TOOLS_ENABLED
#include "core/os/keyboard.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/inspector_dock.h"
#include "editor/node_dock.h"
#endif

#include <stdint.h>

// Types that will be skipped over (in favor of their base types) when setting up instance bindings.
// This must be a superset of `ignored_types` in bindings_generator.cpp.
const Vector<String> ignored_types = {};

#ifdef TOOLS_ENABLED
static bool _create_project_solution_if_needed() {
	CRASH_COND(CSharpLanguage::get_singleton()->get_godotsharp_editor() == nullptr);
	return CSharpLanguage::get_singleton()->get_godotsharp_editor()->call("CreateProjectSolutionIfNeeded");
}
#endif

CSharpLanguage *CSharpLanguage::singleton = nullptr;

GDExtensionInstanceBindingCallbacks CSharpLanguage::_instance_binding_callbacks = {
	&_instance_binding_create_callback,
	&_instance_binding_free_callback,
	&_instance_binding_reference_callback
};

String CSharpLanguage::get_name() const {
	return "C#";
}

String CSharpLanguage::get_type() const {
	return "CSharpScript";
}

String CSharpLanguage::get_extension() const {
	return "cs";
}

void CSharpLanguage::init() {
#ifdef TOOLS_ENABLED
	if (OS::get_singleton()->get_cmdline_args().find("--generate-mono-glue")) {
		print_verbose(".NET: Skipping runtime initialization because glue generation is enabled.");
		return;
	}
#endif
#ifdef DEBUG_METHODS_ENABLED
	if (OS::get_singleton()->get_cmdline_args().find("--class-db-json")) {
		class_db_api_to_json("user://class_db_api.json", ClassDB::API_CORE);
#ifdef TOOLS_ENABLED
		class_db_api_to_json("user://class_db_api_editor.json", ClassDB::API_EDITOR);
#endif
	}
#endif

	GLOBAL_DEF("dotnet/project/assembly_name", "");
#ifdef TOOLS_ENABLED
	GLOBAL_DEF("dotnet/project/solution_directory", "");
	GLOBAL_DEF(PropertyInfo(Variant::INT, "dotnet/project/assembly_reload_attempts", PROPERTY_HINT_RANGE, "1,16,1,or_greater"), 3);
#endif

#ifdef TOOLS_ENABLED
	EditorNode::add_init_callback(&_editor_init_callback);
#endif

	gdmono = memnew(GDMono);

	// Initialize only if the project uses C#.
	if (gdmono->should_initialize()) {
		gdmono->initialize();
	}
}

void CSharpLanguage::finish() {
	finalize();
}

void CSharpLanguage::finalize() {
	if (finalized) {
		return;
	}

	if (gdmono && gdmono->is_runtime_initialized() && GDMonoCache::godot_api_cache_updated) {
		GDMonoCache::managed_callbacks.DisposablesTracker_OnGodotShuttingDown();
	}

	finalizing = true;

	// Make sure all script binding gchandles are released before finalizing GDMono.
	for (KeyValue<Object *, CSharpScriptBinding> &E : script_bindings) {
		CSharpScriptBinding &script_binding = E.value;

		if (!script_binding.gchandle.is_released()) {
			script_binding.gchandle.release();
			script_binding.inited = false;
		}

		// Make sure we clear all the instance binding callbacks so they don't get called
		// after finalizing the C# language.
		script_binding.owner->free_instance_binding(this);
	}

	if (gdmono) {
		memdelete(gdmono);
		gdmono = nullptr;
	}

	// Clear here, after finalizing all domains to make sure there is nothing else referencing the elements.
	script_bindings.clear();

#ifdef DEBUG_ENABLED
	for (const KeyValue<ObjectID, int> &E : unsafe_object_references) {
		const ObjectID &id = E.key;
		Object *obj = ObjectDB::get_instance(id);

		if (obj) {
			ERR_PRINT("Leaked unsafe reference to object: " + obj->to_string());
		} else {
			ERR_PRINT("Leaked unsafe reference to deleted object: " + itos(id));
		}
	}
#endif

	memdelete(managed_callable_middleman);

	finalizing = false;
	finalized = true;
}

void CSharpLanguage::get_reserved_words(List<String> *p_words) const {
	static const char *_reserved_words[] = {
		// Reserved keywords
		"abstract",
		"as",
		"base",
		"bool",
		"break",
		"byte",
		"case",
		"catch",
		"char",
		"checked",
		"class",
		"const",
		"continue",
		"decimal",
		"default",
		"delegate",
		"do",
		"double",
		"else",
		"enum",
		"event",
		"explicit",
		"extern",
		"false",
		"finally",
		"fixed",
		"float",
		"for",
		"foreach",
		"goto",
		"if",
		"implicit",
		"in",
		"int",
		"interface",
		"internal",
		"is",
		"lock",
		"long",
		"namespace",
		"new",
		"null",
		"object",
		"operator",
		"out",
		"override",
		"params",
		"private",
		"protected",
		"public",
		"readonly",
		"ref",
		"return",
		"sbyte",
		"sealed",
		"short",
		"sizeof",
		"stackalloc",
		"static",
		"string",
		"struct",
		"switch",
		"this",
		"throw",
		"true",
		"try",
		"typeof",
		"uint",
		"ulong",
		"unchecked",
		"unsafe",
		"ushort",
		"using",
		"virtual",
		"void",
		"volatile",
		"while",

		// Contextual keywords. Not reserved words, but I guess we should include
		// them because this seems to be used only for syntax highlighting.
		"add",
		"alias",
		"ascending",
		"async",
		"await",
		"by",
		"descending",
		"dynamic",
		"equals",
		"from",
		"get",
		"global",
		"group",
		"into",
		"join",
		"let",
		"nameof",
		"on",
		"orderby",
		"partial",
		"remove",
		"select",
		"set",
		"value",
		"var",
		"when",
		"where",
		"yield",
		nullptr
	};

	const char **w = _reserved_words;

	while (*w) {
		p_words->push_back(*w);
		w++;
	}
}

bool CSharpLanguage::is_control_flow_keyword(const String &p_keyword) const {
	return p_keyword == "break" ||
			p_keyword == "case" ||
			p_keyword == "catch" ||
			p_keyword == "continue" ||
			p_keyword == "default" ||
			p_keyword == "do" ||
			p_keyword == "else" ||
			p_keyword == "finally" ||
			p_keyword == "for" ||
			p_keyword == "foreach" ||
			p_keyword == "goto" ||
			p_keyword == "if" ||
			p_keyword == "return" ||
			p_keyword == "switch" ||
			p_keyword == "throw" ||
			p_keyword == "try" ||
			p_keyword == "while";
}

void CSharpLanguage::get_comment_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("//"); // single-line comment
	p_delimiters->push_back("/* */"); // delimited comment
}

void CSharpLanguage::get_doc_comment_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("///"); // single-line doc comment
	p_delimiters->push_back("/** */"); // delimited doc comment
}

void CSharpLanguage::get_string_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("' '"); // character literal
	p_delimiters->push_back("\" \""); // regular string literal
	p_delimiters->push_back("@\" \""); // verbatim string literal
	// Generic string highlighting suffices as a workaround for now.
}

static String get_base_class_name(const String &p_base_class_name, const String p_class_name) {
	String base_class = pascal_to_pascal_case(p_base_class_name);
	if (p_class_name == base_class) {
		base_class = "Godot." + base_class;
	}
	return base_class;
}

bool CSharpLanguage::is_using_templates() {
	return true;
}

Ref<Script> CSharpLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	Ref<CSharpScript> scr;
	scr.instantiate();

	String class_name_no_spaces = p_class_name.replace(" ", "_");
	String base_class_name = get_base_class_name(p_base_class_name, class_name_no_spaces);
	String processed_template = p_template;
	processed_template = processed_template.replace("_BINDINGS_NAMESPACE_", BINDINGS_NAMESPACE)
								 .replace("_BASE_", base_class_name)
								 .replace("_CLASS_", class_name_no_spaces)
								 .replace("_TS_", _get_indentation());
	scr->set_source_code(processed_template);
	return scr;
}

Vector<ScriptLanguage::ScriptTemplate> CSharpLanguage::get_built_in_templates(const StringName &p_object) {
	Vector<ScriptLanguage::ScriptTemplate> templates;
#ifdef TOOLS_ENABLED
	for (int i = 0; i < TEMPLATES_ARRAY_SIZE; i++) {
		if (TEMPLATES[i].inherit == p_object) {
			templates.append(TEMPLATES[i]);
		}
	}
#endif
	return templates;
}

String CSharpLanguage::validate_path(const String &p_path) const {
	String class_name = p_path.get_file().get_basename();
	List<String> keywords;
	get_reserved_words(&keywords);
	if (keywords.find(class_name)) {
		return RTR("Class name can't be a reserved keyword");
	}
	if (!TS->is_valid_identifier(class_name)) {
		return RTR("Class name must be a valid identifier");
	}

	return "";
}

Script *CSharpLanguage::create_script() const {
	return memnew(CSharpScript);
}

bool CSharpLanguage::supports_builtin_mode() const {
	return false;
}

ScriptLanguage::ScriptNameCasing CSharpLanguage::preferred_file_name_casing() const {
	return SCRIPT_NAME_CASING_PASCAL_CASE;
}

#ifdef TOOLS_ENABLED
String CSharpLanguage::make_function(const String &, const String &p_name, const PackedStringArray &p_args) const {
	// The make_function() API does not work for C# scripts.
	// It will always append the generated function at the very end of the script. In C#, it will break compilation by
	// appending code after the final closing bracket (either the class' or the namespace's).
	// To prevent issues, we have can_make_function() returning false, and make_function() is never implemented.
	return String();
}
#else
String CSharpLanguage::make_function(const String &, const String &, const PackedStringArray &) const {
	return String();
}
#endif

String CSharpLanguage::_get_indentation() const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		bool use_space_indentation = EDITOR_GET("text_editor/behavior/indent/type");

		if (use_space_indentation) {
			int indent_size = EDITOR_GET("text_editor/behavior/indent/size");
			return String(" ").repeat(indent_size);
		}
	}
#endif
	return "\t";
}

bool CSharpLanguage::handles_global_class_type(const String &p_type) const {
	return p_type == get_type();
}

String CSharpLanguage::get_global_class_name(const String &p_path, String *r_base_type, String *r_icon_path) const {
	String class_name;
	GDMonoCache::managed_callbacks.ScriptManagerBridge_GetGlobalClassName(&p_path, r_base_type, r_icon_path, &class_name);
	return class_name;
}

String CSharpLanguage::debug_get_error() const {
	return _debug_error;
}

int CSharpLanguage::debug_get_stack_level_count() const {
	if (_debug_parse_err_line >= 0) {
		return 1;
	}

	// TODO: StackTrace
	return 1;
}

int CSharpLanguage::debug_get_stack_level_line(int p_level) const {
	if (_debug_parse_err_line >= 0) {
		return _debug_parse_err_line;
	}

	// TODO: StackTrace
	return 1;
}

String CSharpLanguage::debug_get_stack_level_function(int p_level) const {
	if (_debug_parse_err_line >= 0) {
		return String();
	}

	// TODO: StackTrace
	return String();
}

String CSharpLanguage::debug_get_stack_level_source(int p_level) const {
	if (_debug_parse_err_line >= 0) {
		return _debug_parse_err_file;
	}

	// TODO: StackTrace
	return String();
}

Vector<ScriptLanguage::StackInfo> CSharpLanguage::debug_get_current_stack_info() {
#ifdef DEBUG_ENABLED
	// Printing an error here will result in endless recursion, so we must be careful
	static thread_local bool _recursion_flag_ = false;
	if (_recursion_flag_) {
		return Vector<StackInfo>();
	}
	_recursion_flag_ = true;
	SCOPE_EXIT {
		_recursion_flag_ = false; // clang-format off
	}; // clang-format on

	if (!gdmono || !gdmono->is_runtime_initialized()) {
		return Vector<StackInfo>();
	}

	Vector<StackInfo> si;

	if (GDMonoCache::godot_api_cache_updated) {
		GDMonoCache::managed_callbacks.DebuggingUtils_GetCurrentStackInfo(&si);
	}

	return si;
#else
	return Vector<StackInfo>();
#endif
}

void CSharpLanguage::post_unsafe_reference(Object *p_obj) {
#ifdef DEBUG_ENABLED
	MutexLock lock(unsafe_object_references_lock);
	ObjectID id = p_obj->get_instance_id();
	unsafe_object_references[id]++;
#endif
}

void CSharpLanguage::pre_unsafe_unreference(Object *p_obj) {
#ifdef DEBUG_ENABLED
	MutexLock lock(unsafe_object_references_lock);
	ObjectID id = p_obj->get_instance_id();
	HashMap<ObjectID, int>::Iterator elem = unsafe_object_references.find(id);
	ERR_FAIL_NULL(elem);
	if (--elem->value == 0) {
		unsafe_object_references.remove(elem);
	}
#endif
}

void CSharpLanguage::frame() {
	if (gdmono && gdmono->is_runtime_initialized() && GDMonoCache::godot_api_cache_updated) {
		GDMonoCache::managed_callbacks.ScriptManagerBridge_FrameCallback();
	}
}

struct CSharpScriptDepSort {
	// Must support sorting so inheritance works properly (parent must be reloaded first)
	bool operator()(const Ref<CSharpScript> &A, const Ref<CSharpScript> &B) const {
		if (A == B) {
			// Shouldn't happen but just in case...
			return false;
		}
		const Script *I = B->get_base_script().ptr();
		while (I) {
			if (I == A.ptr()) {
				// A is a base of B
				return true;
			}

			I = I->get_base_script().ptr();
		}

		// A isn't a base of B
		return false;
	}
};

void CSharpLanguage::reload_all_scripts() {
#ifdef GD_MONO_HOT_RELOAD
	if (is_assembly_reloading_needed()) {
		reload_assemblies(false);
	}
#endif
}

void CSharpLanguage::reload_scripts(const Array &p_scripts, bool p_soft_reload) {
#ifdef GD_MONO_HOT_RELOAD
	if (is_assembly_reloading_needed()) {
		reload_assemblies(p_soft_reload);
	}
#endif
}

void CSharpLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
	CRASH_COND(!Engine::get_singleton()->is_editor_hint());

#ifdef TOOLS_ENABLED
	get_godotsharp_editor()->get_node(NodePath("HotReloadAssemblyWatcher"))->call("RestartTimer");
#endif

#ifdef GD_MONO_HOT_RELOAD
	if (is_assembly_reloading_needed()) {
		reload_assemblies(p_soft_reload);
	}
#endif
}

#ifdef GD_MONO_HOT_RELOAD
bool CSharpLanguage::is_assembly_reloading_needed() {
	ERR_FAIL_NULL_V(gdmono, false);
	if (!gdmono->is_runtime_initialized()) {
		return false;
	}

	String assembly_path = gdmono->get_project_assembly_path();

	if (!assembly_path.is_empty()) {
		if (!FileAccess::exists(assembly_path)) {
			return false; // No assembly to load
		}

		if (FileAccess::get_modified_time(assembly_path) <= gdmono->get_project_assembly_modified_time()) {
			return false; // Already up to date
		}
	} else {
		String assembly_name = path::get_csharp_project_name();

		assembly_path = GodotSharpDirs::get_res_temp_assemblies_dir()
								.path_join(assembly_name + ".dll");
		assembly_path = ProjectSettings::get_singleton()->globalize_path(assembly_path);

		if (!FileAccess::exists(assembly_path)) {
			return false; // No assembly to load
		}
	}

	return true;
}

void CSharpLanguage::reload_assemblies(bool p_soft_reload) {
	ERR_FAIL_NULL(gdmono);
	if (!gdmono->is_runtime_initialized()) {
		return;
	}

	if (!Engine::get_singleton()->is_editor_hint()) {
		// We disable collectible assemblies in the game player, because the limitations cause
		// issues with mocking libraries. As such, we can only reload assemblies in the editor.
		return;
	}

	print_verbose(".NET: Reloading assemblies...");

	// There is no soft reloading with Mono. It's always hard reloading.

	List<Ref<CSharpScript>> scripts;

	{
		MutexLock lock(script_instances_mutex);

		for (SelfList<CSharpScript> *elem = script_list.first(); elem; elem = elem->next()) {
			// Do not reload scripts with only non-collectible instances to avoid disrupting event subscriptions and such.
			bool is_reloadable = elem->self()->instances.size() == 0;
			for (Object *obj : elem->self()->instances) {
				ERR_CONTINUE(!obj->get_script_instance());
				CSharpInstance *csi = static_cast<CSharpInstance *>(obj->get_script_instance());
				if (GDMonoCache::managed_callbacks.GCHandleBridge_GCHandleIsTargetCollectible(csi->get_gchandle_intptr())) {
					is_reloadable = true;
					break;
				}
			}
			if (is_reloadable) {
				// Cast to CSharpScript to avoid being erased by accident.
				scripts.push_back(Ref<CSharpScript>(elem->self()));
			}
		}
	}

	scripts.sort_custom<CSharpScriptDepSort>(); // Update in inheritance dependency order

	// Serialize managed callables
	{
		MutexLock lock(ManagedCallable::instances_mutex);

		for (SelfList<ManagedCallable> *elem = ManagedCallable::instances.first(); elem; elem = elem->next()) {
			ManagedCallable *managed_callable = elem->self();

			ERR_CONTINUE(managed_callable->delegate_handle.value == nullptr);

			if (!GDMonoCache::managed_callbacks.GCHandleBridge_GCHandleIsTargetCollectible(managed_callable->delegate_handle)) {
				continue;
			}

			Array serialized_data;

			bool success = GDMonoCache::managed_callbacks.DelegateUtils_TrySerializeDelegateWithGCHandle(
					managed_callable->delegate_handle, &serialized_data);

			if (success) {
				ManagedCallable::instances_pending_reload.insert(managed_callable, serialized_data);
			} else if (OS::get_singleton()->is_stdout_verbose()) {
				OS::get_singleton()->print("Failed to serialize delegate\n");
			}
		}
	}

	List<Ref<CSharpScript>> to_reload;

	// We need to keep reference instances alive during reloading
	List<Ref<RefCounted>> rc_instances;

	for (const KeyValue<Object *, CSharpScriptBinding> &E : script_bindings) {
		const CSharpScriptBinding &script_binding = E.value;
		RefCounted *rc = Object::cast_to<RefCounted>(script_binding.owner);
		if (rc) {
			rc_instances.push_back(Ref<RefCounted>(rc));
		}
	}

	// As scripts are going to be reloaded, must proceed without locking here

	for (Ref<CSharpScript> &scr : scripts) {
		// If someone removes a script from a node, deletes the script, builds, adds a script to the
		// same node, then builds again, the script might have no path and also no script_class. In
		// that case, we can't (and don't need to) reload it.
		if (scr->get_path().is_empty() && !scr->valid) {
			continue;
		}

		to_reload.push_back(scr);

		// Script::instances are deleted during managed object disposal, which happens on domain finalize.
		// Only placeholders are kept. Therefore we need to keep a copy before that happens.

		for (Object *obj : scr->instances) {
			scr->pending_reload_instances.insert(obj->get_instance_id());

			// Since this script instance wasn't a placeholder, add it to the list of placeholders
			// that will have to be eventually replaced with a script instance in case it turns into one.
			// This list is not cleared after the reload and the collected instances only leave
			// the list if the script is instantiated or if it was a tool script but becomes a
			// non-tool script in a rebuild.
			scr->pending_replace_placeholders.insert(obj->get_instance_id());

			RefCounted *rc = Object::cast_to<RefCounted>(obj);
			if (rc) {
				rc_instances.push_back(Ref<RefCounted>(rc));
			}
		}

#ifdef TOOLS_ENABLED
		for (PlaceHolderScriptInstance *instance : scr->placeholders) {
			Object *obj = instance->get_owner();
			scr->pending_reload_instances.insert(obj->get_instance_id());

			RefCounted *rc = Object::cast_to<RefCounted>(obj);
			if (rc) {
				rc_instances.push_back(Ref<RefCounted>(rc));
			}
		}
#endif

		// Save state and remove script from instances
		RBMap<ObjectID, CSharpScript::StateBackup> &owners_map = scr->pending_reload_state;

		for (Object *obj : scr->instances) {
			ERR_CONTINUE(!obj->get_script_instance());

			CSharpInstance *csi = static_cast<CSharpInstance *>(obj->get_script_instance());

			// Call OnBeforeSerialize and save instance info

			CSharpScript::StateBackup state;

			Dictionary properties;

			GDMonoCache::managed_callbacks.CSharpInstanceBridge_SerializeState(
					csi->get_gchandle_intptr(), &properties, &state.event_signals);

			for (const Variant *s = properties.next(nullptr); s != nullptr; s = properties.next(s)) {
				StringName name = *s;
				Variant value = properties[*s];
				state.properties.push_back(Pair<StringName, Variant>(name, value));
			}

			owners_map[obj->get_instance_id()] = state;
		}
	}

	// After the state of all instances is saved, clear scripts and script instances
	for (Ref<CSharpScript> &scr : scripts) {
		while (scr->instances.begin()) {
			Object *obj = *scr->instances.begin();
			obj->set_script(Ref<RefCounted>()); // Remove script and existing script instances (placeholder are not removed before domain reload)
		}

		scr->was_tool_before_reload = scr->type_info.is_tool;
		scr->_clear();
	}

	// Release the delegates that were serialized earlier.
	{
		MutexLock lock(ManagedCallable::instances_mutex);

		for (KeyValue<ManagedCallable *, Array> &kv : ManagedCallable::instances_pending_reload) {
			kv.key->release_delegate_handle();
		}
	}

	// Do domain reload
	if (gdmono->reload_project_assemblies() != OK) {
		// Failed to reload the scripts domain
		// Make sure to add the scripts back to their owners before returning
		for (Ref<CSharpScript> &scr : to_reload) {
			for (const KeyValue<ObjectID, CSharpScript::StateBackup> &F : scr->pending_reload_state) {
				Object *obj = ObjectDB::get_instance(F.key);

				if (!obj) {
					continue;
				}

				ObjectID obj_id = obj->get_instance_id();

				// Use a placeholder for now to avoid losing the state when saving a scene

				PlaceHolderScriptInstance *placeholder = scr->placeholder_instance_create(obj);
				obj->set_script_instance(placeholder);

#ifdef TOOLS_ENABLED
				// Even though build didn't fail, this tells the placeholder to keep properties and
				// it allows using property_set_fallback for restoring the state without a valid script.
				scr->placeholder_fallback_enabled = true;
#endif

				// Restore Variant properties state, it will be kept by the placeholder until the next script reloading
				for (const Pair<StringName, Variant> &G : scr->pending_reload_state[obj_id].properties) {
					placeholder->property_set_fallback(G.first, G.second, nullptr);
				}

				scr->pending_reload_state.erase(obj_id);
			}

			scr->pending_reload_instances.clear();
			scr->pending_reload_state.clear();
		}

		return;
	}

	List<Ref<CSharpScript>> to_reload_state;

	for (Ref<CSharpScript> &scr : to_reload) {
#ifdef TOOLS_ENABLED
		scr->exports_invalidated = true;
#endif

		if (!scr->get_path().is_empty() && !scr->get_path().begins_with("csharp://")) {
			scr->reload(p_soft_reload);

			if (!scr->valid) {
				scr->pending_reload_instances.clear();
				scr->pending_reload_state.clear();
				continue;
			}
		} else {
			bool success = GDMonoCache::managed_callbacks.ScriptManagerBridge_TryReloadRegisteredScriptWithClass(scr.ptr());

			if (!success) {
				// Couldn't reload
				scr->pending_reload_instances.clear();
				scr->pending_reload_state.clear();
				continue;
			}
		}

		StringName native_name = scr->get_instance_base_type();

		{
			for (const ObjectID &obj_id : scr->pending_reload_instances) {
				Object *obj = ObjectDB::get_instance(obj_id);

				if (!obj) {
					scr->pending_reload_state.erase(obj_id);
					continue;
				}

				if (!ClassDB::is_parent_class(obj->get_class_name(), native_name)) {
					// No longer inherits the same compatible type, can't reload
					scr->pending_reload_state.erase(obj_id);
					continue;
				}

				ScriptInstance *si = obj->get_script_instance();

				// Check if the script must be instantiated or kept as a placeholder
				// when the script may not be a tool (see #65266)
				bool replace_placeholder = scr->pending_replace_placeholders.has(obj->get_instance_id());
				if (!scr->is_tool() && scr->was_tool_before_reload) {
					// The script was a tool before the rebuild so the removal was intentional.
					replace_placeholder = false;
					scr->pending_replace_placeholders.erase(obj->get_instance_id());
				}

#ifdef TOOLS_ENABLED
				if (si) {
					// If the script instance is not null, then it must be a placeholder.
					// Non-placeholder script instances are removed in godot_icall_Object_Disposed.
					CRASH_COND(!si->is_placeholder());

					if (replace_placeholder || scr->is_tool() || ScriptServer::is_scripting_enabled()) {
						// Replace placeholder with a script instance.

						CSharpScript::StateBackup &state_backup = scr->pending_reload_state[obj_id];

						// Backup placeholder script instance state before replacing it with a script instance.
						si->get_property_state(state_backup.properties);

						ScriptInstance *instance = scr->instance_create(obj);

						if (instance) {
							scr->placeholders.erase(static_cast<PlaceHolderScriptInstance *>(si));
							scr->pending_replace_placeholders.erase(obj->get_instance_id());
							obj->set_script_instance(instance);
						}
					}

					continue;
				}
#else
				CRASH_COND(si != nullptr);
#endif

				// Re-create the script instance.
				if (replace_placeholder || scr->is_tool() || ScriptServer::is_scripting_enabled()) {
					// Create script instance or replace placeholder with a script instance.
					ScriptInstance *instance = scr->instance_create(obj);

					if (instance) {
						scr->pending_replace_placeholders.erase(obj->get_instance_id());
						obj->set_script_instance(instance);
						continue;
					}
				}
				// The script instance could not be instantiated or wasn't in the list of placeholders to replace.
				obj->set_script(scr);
#ifdef DEBUG_ENABLED
				// If we reached here, the instantiated script must be a placeholder.
				CRASH_COND(!obj->get_script_instance()->is_placeholder());
#endif
			}
		}

		to_reload_state.push_back(scr);
	}

	// Deserialize managed callables.
	// This is done before reloading script's internal state, so potential callables invoked in properties work.
	{
		MutexLock lock(ManagedCallable::instances_mutex);

		for (const KeyValue<ManagedCallable *, Array> &elem : ManagedCallable::instances_pending_reload) {
			ManagedCallable *managed_callable = elem.key;
			const Array &serialized_data = elem.value;

			GCHandleIntPtr delegate = { nullptr };

			bool success = GDMonoCache::managed_callbacks.DelegateUtils_TryDeserializeDelegateWithGCHandle(
					&serialized_data, &delegate);

			if (success) {
				ERR_CONTINUE(delegate.value == nullptr);
				managed_callable->delegate_handle = delegate;
			} else if (OS::get_singleton()->is_stdout_verbose()) {
				OS::get_singleton()->print("Failed to deserialize delegate\n");
			}
		}

		ManagedCallable::instances_pending_reload.clear();
	}

	for (Ref<CSharpScript> &scr : to_reload_state) {
		for (const ObjectID &obj_id : scr->pending_reload_instances) {
			Object *obj = ObjectDB::get_instance(obj_id);

			if (!obj) {
				scr->pending_reload_state.erase(obj_id);
				continue;
			}

			ERR_CONTINUE(!obj->get_script_instance());

			CSharpScript::StateBackup &state_backup = scr->pending_reload_state[obj_id];

			CSharpInstance *csi = CAST_CSHARP_INSTANCE(obj->get_script_instance());

			if (csi) {
				Dictionary properties;

				for (const Pair<StringName, Variant> &G : state_backup.properties) {
					properties[G.first] = G.second;
				}

				// Restore serialized state and call OnAfterDeserialize.
				GDMonoCache::managed_callbacks.CSharpInstanceBridge_DeserializeState(
						csi->get_gchandle_intptr(), &properties, &state_backup.event_signals);
			}
		}

		scr->pending_reload_instances.clear();
		scr->pending_reload_state.clear();
	}

#ifdef TOOLS_ENABLED
	// FIXME: Hack to refresh editor in order to display new properties and signals. See if there is a better alternative.
	if (Engine::get_singleton()->is_editor_hint()) {
		InspectorDock::get_inspector_singleton()->update_tree();
		NodeDock::get_singleton()->update_lists();
	}
#endif
}
#endif

void CSharpLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("cs");
}

#ifdef TOOLS_ENABLED
Error CSharpLanguage::open_in_external_editor(const Ref<Script> &p_script, int p_line, int p_col) {
	return (Error)(int)get_godotsharp_editor()->call("OpenInExternalEditor", p_script, p_line, p_col);
}

bool CSharpLanguage::overrides_external_editor() {
	return get_godotsharp_editor()->call("OverridesExternalEditor");
}
#endif

bool CSharpLanguage::debug_break_parse(const String &p_file, int p_line, const String &p_error) {
	// Not a parser error in our case, but it's still used for other type of errors
	if (EngineDebugger::is_active() && Thread::get_caller_id() == Thread::get_main_id()) {
		_debug_parse_err_line = p_line;
		_debug_parse_err_file = p_file;
		_debug_error = p_error;
		EngineDebugger::get_script_debugger()->debug(this, false, true);
		return true;
	} else {
		return false;
	}
}

bool CSharpLanguage::debug_break(const String &p_error, bool p_allow_continue) {
	if (EngineDebugger::is_active() && Thread::get_caller_id() == Thread::get_main_id()) {
		_debug_parse_err_line = -1;
		_debug_parse_err_file = "";
		_debug_error = p_error;
		EngineDebugger::get_script_debugger()->debug(this, p_allow_continue);
		return true;
	} else {
		return false;
	}
}

#ifdef TOOLS_ENABLED
void CSharpLanguage::_editor_init_callback() {
	// Load GodotTools and initialize GodotSharpEditor

	int32_t interop_funcs_size = 0;
	const void **interop_funcs = godotsharp::get_editor_interop_funcs(interop_funcs_size);

	Object *editor_plugin_obj = GDMono::get_singleton()->get_plugin_callbacks().LoadToolsAssemblyCallback(
			GodotSharpDirs::get_data_editor_tools_dir().path_join("GodotTools.dll").utf16(),
			interop_funcs, interop_funcs_size);
	CRASH_COND(editor_plugin_obj == nullptr);

	EditorPlugin *godotsharp_editor = Object::cast_to<EditorPlugin>(editor_plugin_obj);
	CRASH_COND(godotsharp_editor == nullptr);

	// Add plugin to EditorNode and enable it
	EditorNode::add_editor_plugin(godotsharp_editor);
	godotsharp_editor->enable_plugin();

	get_singleton()->godotsharp_editor = godotsharp_editor;
}
#endif

void CSharpLanguage::set_language_index(int p_idx) {
	ERR_FAIL_COND(lang_idx != -1);
	lang_idx = p_idx;
}

void CSharpLanguage::release_script_gchandle(MonoGCHandleData &p_gchandle) {
	if (!p_gchandle.is_released()) { // Do not lock unnecessarily
		MutexLock lock(get_singleton()->script_gchandle_release_mutex);
		p_gchandle.release();
	}
}

void CSharpLanguage::release_script_gchandle_thread_safe(GCHandleIntPtr p_gchandle_to_free, MonoGCHandleData &r_gchandle) {
	if (!r_gchandle.is_released() && r_gchandle.get_intptr() == p_gchandle_to_free) { // Do not lock unnecessarily
		MutexLock lock(get_singleton()->script_gchandle_release_mutex);
		if (!r_gchandle.is_released() && r_gchandle.get_intptr() == p_gchandle_to_free) {
			r_gchandle.release();
		}
	}
}

void CSharpLanguage::release_binding_gchandle_thread_safe(GCHandleIntPtr p_gchandle_to_free, CSharpScriptBinding &r_script_binding) {
	MonoGCHandleData &gchandle = r_script_binding.gchandle;
	if (!gchandle.is_released() && gchandle.get_intptr() == p_gchandle_to_free) { // Do not lock unnecessarily
		MutexLock lock(get_singleton()->script_gchandle_release_mutex);
		if (!gchandle.is_released() && gchandle.get_intptr() == p_gchandle_to_free) {
			gchandle.release();
			r_script_binding.inited = false; // Here too, to be thread safe
		}
	}
}

CSharpLanguage::CSharpLanguage() {
	ERR_FAIL_COND_MSG(singleton, "C# singleton already exist.");
	singleton = this;
}

CSharpLanguage::~CSharpLanguage() {
	finalize();
	singleton = nullptr;
}

bool CSharpLanguage::setup_csharp_script_binding(CSharpScriptBinding &r_script_binding, Object *p_object) {
#ifdef DEBUG_ENABLED
	// I don't trust you
	if (p_object->get_script_instance()) {
		CSharpInstance *csharp_instance = CAST_CSHARP_INSTANCE(p_object->get_script_instance());
		CRASH_COND(csharp_instance != nullptr && !csharp_instance->is_destructing_script_instance());
	}
#endif

	StringName type_name = p_object->get_class_name();

	const ClassDB::ClassInfo *classinfo = ClassDB::classes.getptr(type_name);

	// This skipping of GDExtension classes, as well as whatever classes are in this list of ignored types, is a
	// workaround to allow GDExtension classes to be used from C# so long as they're only used through base classes that
	// are registered from the engine. This will likely need to be removed whenever proper support for GDExtension
	// classes is added to C#. See #75955 for more details.
	while (classinfo && (!classinfo->exposed || classinfo->gdextension || ignored_types.has(classinfo->name))) {
		classinfo = classinfo->inherits_ptr;
	}

	ERR_FAIL_NULL_V(classinfo, false);
	type_name = classinfo->name;

	bool parent_is_object_class = ClassDB::is_parent_class(p_object->get_class_name(), type_name);
	ERR_FAIL_COND_V_MSG(!parent_is_object_class, false,
			"Type inherits from native type '" + type_name + "', so it can't be instantiated in object of type: '" + p_object->get_class() + "'.");

#ifdef DEBUG_ENABLED
	CRASH_COND(!r_script_binding.gchandle.is_released());
#endif

	GCHandleIntPtr strong_gchandle =
			GDMonoCache::managed_callbacks.ScriptManagerBridge_CreateManagedForGodotObjectBinding(
					&type_name, p_object);

	ERR_FAIL_NULL_V(strong_gchandle.value, false);

	r_script_binding.inited = true;
	r_script_binding.type_name = type_name;
	r_script_binding.gchandle = MonoGCHandleData(strong_gchandle, gdmono::GCHandleType::STRONG_HANDLE);
	r_script_binding.owner = p_object;

	// Tie managed to unmanaged
	RefCounted *rc = Object::cast_to<RefCounted>(p_object);

	if (rc) {
		// Unsafe refcount increment. The managed instance also counts as a reference.
		// This way if the unmanaged world has no references to our owner
		// but the managed instance is alive, the refcount will be 1 instead of 0.
		// See: godot_icall_RefCounted_Dtor(MonoObject *p_obj, Object *p_ptr)

		rc->reference();
		CSharpLanguage::get_singleton()->post_unsafe_reference(rc);
	}

	return true;
}

RBMap<Object *, CSharpScriptBinding>::Element *CSharpLanguage::insert_script_binding(Object *p_object, const CSharpScriptBinding &p_script_binding) {
	return script_bindings.insert(p_object, p_script_binding);
}

void *CSharpLanguage::_instance_binding_create_callback(void *, void *p_instance) {
	CSharpLanguage *csharp_lang = CSharpLanguage::get_singleton();

	MutexLock lock(csharp_lang->language_bind_mutex);

	RBMap<Object *, CSharpScriptBinding>::Element *match = csharp_lang->script_bindings.find((Object *)p_instance);
	if (match) {
		return (void *)match;
	}

	CSharpScriptBinding script_binding;

	return (void *)csharp_lang->insert_script_binding((Object *)p_instance, script_binding);
}

void CSharpLanguage::_instance_binding_free_callback(void *, void *, void *p_binding) {
	CSharpLanguage *csharp_lang = CSharpLanguage::get_singleton();

	if (GDMono::get_singleton() == nullptr) {
#ifdef DEBUG_ENABLED
		CRASH_COND(csharp_lang && !csharp_lang->script_bindings.is_empty());
#endif
		// Mono runtime finalized, all the gchandle bindings were already released
		return;
	}

	if (csharp_lang->finalizing) {
		return; // inside CSharpLanguage::finish(), all the gchandle bindings are released there
	}

	{
		MutexLock lock(csharp_lang->language_bind_mutex);

		RBMap<Object *, CSharpScriptBinding>::Element *data = (RBMap<Object *, CSharpScriptBinding>::Element *)p_binding;

		CSharpScriptBinding &script_binding = data->value();

		if (script_binding.inited) {
			// Set the native instance field to IntPtr.Zero, if not yet garbage collected.
			// This is done to avoid trying to dispose the native instance from Dispose(bool).
			GDMonoCache::managed_callbacks.ScriptManagerBridge_SetGodotObjectPtr(
					script_binding.gchandle.get_intptr(), nullptr);

			script_binding.gchandle.release();
			script_binding.inited = false;
		}

		csharp_lang->script_bindings.erase(data);
	}
}

GDExtensionBool CSharpLanguage::_instance_binding_reference_callback(void *p_token, void *p_binding, GDExtensionBool p_reference) {
	// Instance bindings callbacks can only be called if the C# language is available.
	// Failing this assert usually means that we didn't clear the instance binding in some Object
	// and the C# language has already been finalized.
	DEV_ASSERT(CSharpLanguage::get_singleton() != nullptr);

	CRASH_COND(!p_binding);

	CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)p_binding)->get();

	RefCounted *rc_owner = Object::cast_to<RefCounted>(script_binding.owner);

#ifdef DEBUG_ENABLED
	CRASH_COND(!rc_owner);
#endif

	MonoGCHandleData &gchandle = script_binding.gchandle;

	int refcount = rc_owner->get_reference_count();

	if (!script_binding.inited) {
		return refcount == 0;
	}

	if (p_reference) {
		// Refcount incremented
		if (refcount > 1 && gchandle.is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
			// The reference count was increased after the managed side was the only one referencing our owner.
			// This means the owner is being referenced again by the unmanaged side,
			// so the owner must hold the managed side alive again to avoid it from being GCed.

			// Release the current weak handle and replace it with a strong handle.

			GCHandleIntPtr old_gchandle = gchandle.get_intptr();
			gchandle.handle = { nullptr }; // No longer owns the handle (released by swap function)

			GCHandleIntPtr new_gchandle = { nullptr };
			bool create_weak = false;
			bool target_alive = GDMonoCache::managed_callbacks.ScriptManagerBridge_SwapGCHandleForType(
					old_gchandle, &new_gchandle, create_weak);

			if (!target_alive) {
				return false; // Called after the managed side was collected, so nothing to do here
			}

			gchandle = MonoGCHandleData(new_gchandle, gdmono::GCHandleType::STRONG_HANDLE);
		}

		return false;
	} else {
		// Refcount decremented
		if (refcount == 1 && !gchandle.is_released() && !gchandle.is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
			// If owner owner is no longer referenced by the unmanaged side,
			// the managed instance takes responsibility of deleting the owner when GCed.

			// Release the current strong handle and replace it with a weak handle.

			GCHandleIntPtr old_gchandle = gchandle.get_intptr();
			gchandle.handle = { nullptr }; // No longer owns the handle (released by swap function)

			GCHandleIntPtr new_gchandle = { nullptr };
			bool create_weak = true;
			bool target_alive = GDMonoCache::managed_callbacks.ScriptManagerBridge_SwapGCHandleForType(
					old_gchandle, &new_gchandle, create_weak);

			if (!target_alive) {
				return refcount == 0; // Called after the managed side was collected, so nothing to do here
			}

			gchandle = MonoGCHandleData(new_gchandle, gdmono::GCHandleType::WEAK_HANDLE);

			return false;
		}

		return refcount == 0;
	}
}

void *CSharpLanguage::get_instance_binding(Object *p_object) {
	return p_object->get_instance_binding(get_singleton(), &_instance_binding_callbacks);
}

void *CSharpLanguage::get_instance_binding_with_setup(Object *p_object) {
	void *binding = get_instance_binding(p_object);

	// Initially this was in `_instance_binding_create_callback`. However, after the new instance
	// binding re-write it was resulting in a deadlock in `_instance_binding_reference`, as
	// `setup_csharp_script_binding` may call `reference()`. It was moved here outside to fix that.

	if (binding) {
		CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)binding)->value();

		if (!script_binding.inited) {
			MutexLock lock(CSharpLanguage::get_singleton()->get_language_bind_mutex());

			if (!script_binding.inited) { // Another thread may have set it up
				CSharpLanguage::get_singleton()->setup_csharp_script_binding(script_binding, p_object);
			}
		}
	}

	return binding;
}

void *CSharpLanguage::get_existing_instance_binding(Object *p_object) {
#ifdef DEBUG_ENABLED
	CRASH_COND(p_object->has_instance_binding(p_object));
#endif
	return get_instance_binding(p_object);
}

bool CSharpLanguage::has_instance_binding(Object *p_object) {
	return p_object->has_instance_binding(get_singleton());
}
void CSharpLanguage::tie_native_managed_to_unmanaged(GCHandleIntPtr p_gchandle_intptr, Object *p_unmanaged, const StringName *p_native_name, bool p_ref_counted) {
	// This method should not fail

	CRASH_COND(!p_unmanaged);

	// All mono objects created from the managed world (e.g.: 'new Player()')
	// need to have a CSharpScript in order for their methods to be callable from the unmanaged side

	RefCounted *rc = Object::cast_to<RefCounted>(p_unmanaged);

	CRASH_COND(p_ref_counted != (bool)rc);

	MonoGCHandleData gchandle = MonoGCHandleData(p_gchandle_intptr,
			p_ref_counted ? gdmono::GCHandleType::WEAK_HANDLE : gdmono::GCHandleType::STRONG_HANDLE);

	// If it's just a wrapper Godot class and not a custom inheriting class, then attach a
	// script binding instead. One of the advantages of this is that if a script is attached
	// later and it's not a C# script, then the managed object won't have to be disposed.
	// Another reason for doing this is that this instance could outlive CSharpLanguage, which would
	// be problematic when using a script. See: https://github.com/godotengine/godot/issues/25621

	if (p_ref_counted) {
		// Unsafe refcount increment. The managed instance also counts as a reference.
		// This way if the unmanaged world has no references to our owner
		// but the managed instance is alive, the refcount will be 1 instead of 0.
		// See: godot_icall_RefCounted_Dtor(MonoObject *p_obj, Object *p_ptr)

		// May not me referenced yet, so we must use init_ref() instead of reference()
		if (rc->init_ref()) {
			CSharpLanguage::get_singleton()->post_unsafe_reference(rc);
		}
	}

	// The object was just created, no script instance binding should have been attached
	CRASH_COND(CSharpLanguage::has_instance_binding(p_unmanaged));

	void *binding = CSharpLanguage::get_singleton()->get_instance_binding(p_unmanaged);

	CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)binding)->value();
	script_binding.inited = true;
	script_binding.type_name = *p_native_name;
	script_binding.gchandle = gchandle;
	script_binding.owner = p_unmanaged;
}

void CSharpLanguage::tie_user_managed_to_unmanaged(GCHandleIntPtr p_gchandle_intptr, Object *p_unmanaged, Ref<CSharpScript> *p_script, bool p_ref_counted) {
	// This method should not fail

	Ref<CSharpScript> script = *p_script;
	// We take care of destructing this reference here, so the managed code won't need to do another P/Invoke call
	p_script->~Ref();

	CRASH_COND(!p_unmanaged);

	// All mono objects created from the managed world (e.g.: 'new Player()')
	// need to have a CSharpScript in order for their methods to be callable from the unmanaged side

	RefCounted *rc = Object::cast_to<RefCounted>(p_unmanaged);

	CRASH_COND(p_ref_counted != (bool)rc);

	MonoGCHandleData gchandle = MonoGCHandleData(p_gchandle_intptr,
			p_ref_counted ? gdmono::GCHandleType::WEAK_HANDLE : gdmono::GCHandleType::STRONG_HANDLE);

	CRASH_COND(script.is_null());

	CSharpInstance *csharp_instance = CSharpInstance::create_for_managed_type(p_unmanaged, script.ptr(), gchandle);

	p_unmanaged->set_script_and_instance(script, csharp_instance);

	csharp_instance->connect_event_signals();
}

void CSharpLanguage::tie_managed_to_unmanaged_with_pre_setup(GCHandleIntPtr p_gchandle_intptr, Object *p_unmanaged) {
	// This method should not fail

	CRASH_COND(!p_unmanaged);

	CSharpInstance *instance = CAST_CSHARP_INSTANCE(p_unmanaged->get_script_instance());

	if (!instance) {
		// Native bindings don't need post-setup
		return;
	}

	CRASH_COND(!instance->gchandle.is_released());

	// Tie managed to unmanaged
	instance->gchandle = MonoGCHandleData(p_gchandle_intptr, gdmono::GCHandleType::STRONG_HANDLE);

	if (instance->base_ref_counted) {
		instance->_reference_owner_unsafe(); // Here, after assigning the gchandle (for the refcount_incremented callback)
	}

	{
		MutexLock lock(CSharpLanguage::get_singleton()->get_script_instances_mutex());
		// instances is a set, so it's safe to insert multiple times (e.g.: from _internal_new_managed)
		instance->script->instances.insert(instance->owner);
	}

	instance->connect_event_signals();
}

CSharpInstance *CSharpInstance::create_for_managed_type(Object *p_owner, CSharpScript *p_script, const MonoGCHandleData &p_gchandle) {
	CSharpInstance *instance = memnew(CSharpInstance(Ref<CSharpScript>(p_script)));

	RefCounted *rc = Object::cast_to<RefCounted>(p_owner);

	instance->base_ref_counted = rc != nullptr;
	instance->owner = p_owner;
	instance->gchandle = p_gchandle;

	if (instance->base_ref_counted) {
		instance->_reference_owner_unsafe();
	}

	{
		MutexLock lock(CSharpLanguage::get_singleton()->get_script_instances_mutex());
		p_script->instances.insert(p_owner);
	}

	return instance;
}

Object *CSharpInstance::get_owner() {
	return owner;
}

bool CSharpInstance::set(const StringName &p_name, const Variant &p_value) {
	ERR_FAIL_COND_V(!script.is_valid(), false);

	return GDMonoCache::managed_callbacks.CSharpInstanceBridge_Set(
			gchandle.get_intptr(), &p_name, &p_value);
}

bool CSharpInstance::get(const StringName &p_name, Variant &r_ret) const {
	ERR_FAIL_COND_V(!script.is_valid(), false);

	Variant ret_value;

	bool ret = GDMonoCache::managed_callbacks.CSharpInstanceBridge_Get(
			gchandle.get_intptr(), &p_name, &ret_value);

	if (ret) {
		r_ret = ret_value;
		return true;
	}

	return false;
}

void CSharpInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	List<PropertyInfo> props;
	ERR_FAIL_COND(!script.is_valid());
#ifdef TOOLS_ENABLED
	for (const PropertyInfo &prop : script->exported_members_cache) {
		props.push_back(prop);
	}
#else
	for (const KeyValue<StringName, PropertyInfo> &E : script->member_info) {
		props.push_front(E.value);
	}
#endif

	for (PropertyInfo &prop : props) {
		validate_property(prop);
		p_properties->push_back(prop);
	}

	// Call _get_property_list

	StringName method = SNAME("_get_property_list");

	Variant ret;
	Callable::CallError call_error;
	bool ok = GDMonoCache::managed_callbacks.CSharpInstanceBridge_Call(
			gchandle.get_intptr(), &method, nullptr, 0, &call_error, &ret);

	// CALL_ERROR_INVALID_METHOD would simply mean it was not overridden
	if (call_error.error != Callable::CallError::CALL_ERROR_INVALID_METHOD) {
		if (call_error.error != Callable::CallError::CALL_OK) {
			ERR_PRINT("Error calling '_get_property_list': " + Variant::get_call_error_text(method, nullptr, 0, call_error));
		} else if (!ok) {
			ERR_PRINT("Unexpected error calling '_get_property_list'");
		} else {
			Array array = ret;
			for (int i = 0, size = array.size(); i < size; i++) {
				p_properties->push_back(PropertyInfo::from_dict(array.get(i)));
			}
		}
	}

	CSharpScript *top = script.ptr()->base_script.ptr();
	while (top != nullptr) {
		props.clear();
#ifdef TOOLS_ENABLED
		for (const PropertyInfo &prop : top->exported_members_cache) {
			props.push_back(prop);
		}
#else
		for (const KeyValue<StringName, PropertyInfo> &E : top->member_info) {
			props.push_front(E.value);
		}
#endif

		for (PropertyInfo &prop : props) {
			validate_property(prop);
			p_properties->push_back(prop);
		}

		top = top->base_script.ptr();
	}
}

Variant::Type CSharpInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	if (script->member_info.has(p_name)) {
		if (r_is_valid) {
			*r_is_valid = true;
		}
		return script->member_info[p_name].type;
	}

	if (r_is_valid) {
		*r_is_valid = false;
	}

	return Variant::NIL;
}

bool CSharpInstance::property_can_revert(const StringName &p_name) const {
	ERR_FAIL_COND_V(!script.is_valid(), false);

	Variant name_arg = p_name;
	const Variant *args[1] = { &name_arg };

	Variant ret;
	Callable::CallError call_error;
	GDMonoCache::managed_callbacks.CSharpInstanceBridge_Call(
			gchandle.get_intptr(), &SNAME("_property_can_revert"), args, 1, &call_error, &ret);

	if (call_error.error != Callable::CallError::CALL_OK) {
		return false;
	}

	return (bool)ret;
}

void CSharpInstance::validate_property(PropertyInfo &p_property) const {
	ERR_FAIL_COND(!script.is_valid());

	Variant property_arg = (Dictionary)p_property;
	const Variant *args[1] = { &property_arg };

	Variant ret;
	Callable::CallError call_error;
	GDMonoCache::managed_callbacks.CSharpInstanceBridge_Call(
			gchandle.get_intptr(), &SNAME("_validate_property"), args, 1, &call_error, &ret);

	if (call_error.error != Callable::CallError::CALL_OK) {
		return;
	}

	p_property = PropertyInfo::from_dict(property_arg);
}

bool CSharpInstance::property_get_revert(const StringName &p_name, Variant &r_ret) const {
	ERR_FAIL_COND_V(!script.is_valid(), false);

	Variant name_arg = p_name;
	const Variant *args[1] = { &name_arg };

	Variant ret;
	Callable::CallError call_error;
	GDMonoCache::managed_callbacks.CSharpInstanceBridge_Call(
			gchandle.get_intptr(), &SNAME("_property_get_revert"), args, 1, &call_error, &ret);

	if (call_error.error != Callable::CallError::CALL_OK) {
		return false;
	}

	r_ret = ret;
	return true;
}

void CSharpInstance::get_method_list(List<MethodInfo> *p_list) const {
	if (!script->is_valid() || !script->valid) {
		return;
	}

	script->get_script_method_list(p_list);
}

bool CSharpInstance::has_method(const StringName &p_method) const {
	if (!script.is_valid()) {
		return false;
	}

	if (!GDMonoCache::godot_api_cache_updated) {
		return false;
	}

	return GDMonoCache::managed_callbacks.CSharpInstanceBridge_HasMethodUnknownParams(
			gchandle.get_intptr(), &p_method);
}

int CSharpInstance::get_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	if (!script->is_valid() || !script->valid) {
		if (r_is_valid) {
			*r_is_valid = false;
		}
		return 0;
	}

	const CSharpScript *top = script.ptr();
	while (top != nullptr) {
		for (const CSharpScript::CSharpMethodInfo &E : top->methods) {
			if (E.name == p_method) {
				if (r_is_valid) {
					*r_is_valid = true;
				}
				return E.method_info.arguments.size();
			}
		}

		top = top->base_script.ptr();
	}

	if (r_is_valid) {
		*r_is_valid = false;
	}
	return 0;
}

Variant CSharpInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	ERR_FAIL_COND_V(!script.is_valid(), Variant());

	Variant ret;
	GDMonoCache::managed_callbacks.CSharpInstanceBridge_Call(
			gchandle.get_intptr(), &p_method, p_args, p_argcount, &r_error, &ret);

	return ret;
}

bool CSharpInstance::_reference_owner_unsafe() {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref_counted);
	CRASH_COND(owner == nullptr);
	CRASH_COND(unsafe_referenced); // already referenced
#endif

	// Unsafe refcount increment. The managed instance also counts as a reference.
	// This way if the unmanaged world has no references to our owner
	// but the managed instance is alive, the refcount will be 1 instead of 0.
	// See: _unreference_owner_unsafe()

	// May not be referenced yet, so we must use init_ref() instead of reference()
	if (static_cast<RefCounted *>(owner)->init_ref()) {
		CSharpLanguage::get_singleton()->post_unsafe_reference(owner);
		unsafe_referenced = true;
	}

	return unsafe_referenced;
}

bool CSharpInstance::_unreference_owner_unsafe() {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref_counted);
	CRASH_COND(owner == nullptr);
#endif

	if (!unsafe_referenced) {
		return false; // Already unreferenced
	}

	unsafe_referenced = false;

	// Called from CSharpInstance::mono_object_disposed() or ~CSharpInstance()

	// Unsafe refcount decrement. The managed instance also counts as a reference.
	// See: _reference_owner_unsafe()

	// Destroying the owner here means self destructing, so we defer the owner destruction to the caller.
	CSharpLanguage::get_singleton()->pre_unsafe_unreference(owner);
	return static_cast<RefCounted *>(owner)->unreference();
}

bool CSharpInstance::_internal_new_managed() {
	CSharpLanguage::get_singleton()->release_script_gchandle(gchandle);

	ERR_FAIL_NULL_V(owner, false);
	ERR_FAIL_COND_V(script.is_null(), false);
	ERR_FAIL_COND_V(!script->can_instantiate(), false);

	bool ok = GDMonoCache::managed_callbacks.ScriptManagerBridge_CreateManagedForGodotObjectScriptInstance(
			script.ptr(), owner, nullptr, 0);

	if (!ok) {
		// Important to clear this before destroying the script instance here
		script = Ref<CSharpScript>();
		owner = nullptr;

		return false;
	}

	CRASH_COND(gchandle.is_released());

	return true;
}

void CSharpInstance::mono_object_disposed(GCHandleIntPtr p_gchandle_to_free) {
	// Must make sure event signals are not left dangling
	disconnect_event_signals();

#ifdef DEBUG_ENABLED
	CRASH_COND(base_ref_counted);
	CRASH_COND(gchandle.is_released());
#endif
	CSharpLanguage::get_singleton()->release_script_gchandle_thread_safe(p_gchandle_to_free, gchandle);
}

void CSharpInstance::mono_object_disposed_baseref(GCHandleIntPtr p_gchandle_to_free, bool p_is_finalizer, bool &r_delete_owner, bool &r_remove_script_instance) {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref_counted);
	CRASH_COND(gchandle.is_released());
#endif

	// Must make sure event signals are not left dangling
	disconnect_event_signals();

	r_remove_script_instance = false;

	if (_unreference_owner_unsafe()) {
		// Safe to self destruct here with memdelete(owner), but it's deferred to the caller to prevent future mistakes.
		r_delete_owner = true;
	} else {
		r_delete_owner = false;
		CSharpLanguage::get_singleton()->release_script_gchandle_thread_safe(p_gchandle_to_free, gchandle);

		if (!p_is_finalizer) {
			// If the native instance is still alive and Dispose() was called
			// (instead of the finalizer), then we remove the script instance.
			r_remove_script_instance = true;
			// TODO: Last usage of 'is_finalizing_scripts_domain'. It should be replaced with a check to determine if the load context is being unloaded.
		} else if (!GDMono::get_singleton()->is_finalizing_scripts_domain()) {
			// If the native instance is still alive and this is called from the finalizer,
			// then it was referenced from another thread before the finalizer could
			// unreference and delete it, so we want to keep it.
			// GC.ReRegisterForFinalize(this) is not safe because the objects referenced by 'this'
			// could have already been collected. Instead we will create a new managed instance here.
			if (!_internal_new_managed()) {
				r_remove_script_instance = true;
			}
		}
	}
}

void CSharpInstance::connect_event_signals() {
	const CSharpScript *top = script.ptr();
	while (top != nullptr && top->valid) {
		for (const CSharpScript::EventSignalInfo &signal : top->event_signals) {
			String signal_name = signal.name;

			// TODO: Use pooling for ManagedCallable instances.
			EventSignalCallable *event_signal_callable = memnew(EventSignalCallable(owner, signal_name));

			Callable callable(event_signal_callable);
			connected_event_signals.push_back(callable);
			owner->connect(signal_name, callable);
		}
		top = top->base_script.ptr();
	}
}

void CSharpInstance::disconnect_event_signals() {
	for (const Callable &callable : connected_event_signals) {
		const EventSignalCallable *event_signal_callable = static_cast<const EventSignalCallable *>(callable.get_custom());
		owner->disconnect(event_signal_callable->get_signal(), callable);
	}

	connected_event_signals.clear();
}

void CSharpInstance::refcount_incremented() {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref_counted);
	CRASH_COND(owner == nullptr);
#endif

	RefCounted *rc_owner = Object::cast_to<RefCounted>(owner);

	if (rc_owner->get_reference_count() > 1 && gchandle.is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
		// The reference count was increased after the managed side was the only one referencing our owner.
		// This means the owner is being referenced again by the unmanaged side,
		// so the owner must hold the managed side alive again to avoid it from being GCed.

		// Release the current weak handle and replace it with a strong handle.

		GCHandleIntPtr old_gchandle = gchandle.get_intptr();
		gchandle.handle = { nullptr }; // No longer owns the handle (released by swap function)

		GCHandleIntPtr new_gchandle = { nullptr };
		bool create_weak = false;
		bool target_alive = GDMonoCache::managed_callbacks.ScriptManagerBridge_SwapGCHandleForType(
				old_gchandle, &new_gchandle, create_weak);

		if (!target_alive) {
			return; // Called after the managed side was collected, so nothing to do here
		}

		gchandle = MonoGCHandleData(new_gchandle, gdmono::GCHandleType::STRONG_HANDLE);
	}
}

bool CSharpInstance::refcount_decremented() {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref_counted);
	CRASH_COND(owner == nullptr);
#endif

	RefCounted *rc_owner = Object::cast_to<RefCounted>(owner);

	int refcount = rc_owner->get_reference_count();

	if (refcount == 1 && !gchandle.is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
		// If owner owner is no longer referenced by the unmanaged side,
		// the managed instance takes responsibility of deleting the owner when GCed.

		// Release the current strong handle and replace it with a weak handle.

		GCHandleIntPtr old_gchandle = gchandle.get_intptr();
		gchandle.handle = { nullptr }; // No longer owns the handle (released by swap function)

		GCHandleIntPtr new_gchandle = { nullptr };
		bool create_weak = true;
		bool target_alive = GDMonoCache::managed_callbacks.ScriptManagerBridge_SwapGCHandleForType(
				old_gchandle, &new_gchandle, create_weak);

		if (!target_alive) {
			return refcount == 0; // Called after the managed side was collected, so nothing to do here
		}

		gchandle = MonoGCHandleData(new_gchandle, gdmono::GCHandleType::WEAK_HANDLE);

		return false;
	}

	ref_dying = (refcount == 0);

	return ref_dying;
}

const Variant CSharpInstance::get_rpc_config() const {
	return script->get_rpc_config();
}

void CSharpInstance::notification(int p_notification, bool p_reversed) {
	if (p_notification == Object::NOTIFICATION_PREDELETE) {
		if (base_ref_counted) {
			// At this point, Dispose() was already called (manually or from the finalizer).
			// The RefCounted wouldn't have reached 0 otherwise, since the managed side
			// references it and Dispose() needs to be called to release it.
			// However, this means C# RefCounted scripts can't receive NOTIFICATION_PREDELETE, but
			// this is likely the case with GDScript as well: https://github.com/godotengine/godot/issues/6784
			return;
		}
	} else if (p_notification == Object::NOTIFICATION_PREDELETE_CLEANUP) {
		// When NOTIFICATION_PREDELETE_CLEANUP is sent, we also take the chance to call Dispose().
		// It's safe to call Dispose() multiple times and NOTIFICATION_PREDELETE_CLEANUP is guaranteed
		// to be sent at least once, which happens right before the call to the destructor.

		predelete_notified = true;

		if (base_ref_counted) {
			// At this point, Dispose() was already called (manually or from the finalizer).
			// The RefCounted wouldn't have reached 0 otherwise, since the managed side
			// references it and Dispose() needs to be called to release it.
			return;
		}

		// NOTIFICATION_PREDELETE_CLEANUP is not sent to scripts.
		// After calling Dispose() the C# instance can no longer be used,
		// so it should be the last thing we do.
		GDMonoCache::managed_callbacks.CSharpInstanceBridge_CallDispose(
				gchandle.get_intptr(), /* okIfNull */ false);

		return;
	}

	_call_notification(p_notification, p_reversed);
}

void CSharpInstance::_call_notification(int p_notification, bool p_reversed) {
	Variant arg = p_notification;
	const Variant *args[1] = { &arg };

	Variant ret;
	Callable::CallError call_error;
	GDMonoCache::managed_callbacks.CSharpInstanceBridge_Call(
			gchandle.get_intptr(), &SNAME("_notification"), args, 1, &call_error, &ret);
}

String CSharpInstance::to_string(bool *r_valid) {
	String res;
	bool valid;

	GDMonoCache::managed_callbacks.CSharpInstanceBridge_CallToString(
			gchandle.get_intptr(), &res, &valid);

	if (r_valid) {
		*r_valid = valid;
	}

	return res;
}

Ref<Script> CSharpInstance::get_script() const {
	return script;
}

ScriptLanguage *CSharpInstance::get_language() {
	return CSharpLanguage::get_singleton();
}

CSharpInstance::CSharpInstance(const Ref<CSharpScript> &p_script) :
		script(p_script) {
}

CSharpInstance::~CSharpInstance() {
	destructing_script_instance = true;

	// Must make sure event signals are not left dangling
	disconnect_event_signals();

	if (!gchandle.is_released()) {
		if (!predelete_notified && !ref_dying) {
			// This destructor is not called from the owners destructor.
			// This could be being called from the owner's set_script_instance method,
			// meaning this script is being replaced with another one. If this is the case,
			// we must call Dispose here, because Dispose calls owner->set_script_instance(nullptr)
			// and that would mess up with the new script instance if called later.

			GDMonoCache::managed_callbacks.CSharpInstanceBridge_CallDispose(
					gchandle.get_intptr(), /* okIfNull */ true);
		}

		gchandle.release(); // Make sure the gchandle is released
	}

	// If not being called from the owner's destructor, and we still hold a reference to the owner
	if (base_ref_counted && !ref_dying && owner && unsafe_referenced) {
		// The owner's script or script instance is being replaced (or removed)

		// Transfer ownership to an "instance binding"

		RefCounted *rc_owner = static_cast<RefCounted *>(owner);

		// We will unreference the owner before referencing it again, so we need to keep it alive
		Ref<RefCounted> scope_keep_owner_alive(rc_owner);
		(void)scope_keep_owner_alive;

		// Unreference the owner here, before the new "instance binding" references it.
		// Otherwise, the unsafe reference debug checks will incorrectly detect a bug.
		bool die = _unreference_owner_unsafe();
		CRASH_COND(die); // `owner_keep_alive` holds a reference, so it can't die

		void *data = CSharpLanguage::get_instance_binding_with_setup(owner);
		CRASH_COND(data == nullptr);
		CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)data)->get();
		CRASH_COND(!script_binding.inited);

#ifdef DEBUG_ENABLED
		// The "instance binding" holds a reference so the refcount should be at least 2 before `scope_keep_owner_alive` goes out of scope
		CRASH_COND(rc_owner->get_reference_count() <= 1);
#endif
	}

	if (script.is_valid() && owner) {
		MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);

#ifdef DEBUG_ENABLED
		// CSharpInstance must not be created unless it's going to be added to the list for sure
		HashSet<Object *>::Iterator match = script->instances.find(owner);
		CRASH_COND(!match);
		script->instances.remove(match);
#else
		script->instances.erase(owner);
#endif
	}
}

#ifdef TOOLS_ENABLED
void CSharpScript::_placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {
	placeholders.erase(p_placeholder);
}
#endif

#ifdef TOOLS_ENABLED
void CSharpScript::_update_exports_values(HashMap<StringName, Variant> &values, List<PropertyInfo> &propnames) {
	for (const KeyValue<StringName, Variant> &E : exported_members_defval_cache) {
		values[E.key] = E.value;
	}

	for (const PropertyInfo &prop_info : exported_members_cache) {
		propnames.push_back(prop_info);
	}

	if (base_script.is_valid()) {
		base_script->_update_exports_values(values, propnames);
	}
}
#endif

void GD_CLR_STDCALL CSharpScript::_add_property_info_list_callback(CSharpScript *p_script, const String *p_current_class_name, void *p_props, int32_t p_count) {
	GDMonoCache::godotsharp_property_info *props = (GDMonoCache::godotsharp_property_info *)p_props;

#ifdef TOOLS_ENABLED
	p_script->exported_members_cache.push_back(PropertyInfo(
			Variant::NIL, p_script->type_info.class_name, PROPERTY_HINT_NONE,
			p_script->get_path(), PROPERTY_USAGE_CATEGORY));
#endif

	for (int i = 0; i < p_count; i++) {
		const GDMonoCache::godotsharp_property_info &prop = props[i];

		StringName name = *reinterpret_cast<const StringName *>(&prop.name);
		String hint_string = *reinterpret_cast<const String *>(&prop.hint_string);

		PropertyInfo pinfo(prop.type, name, prop.hint, hint_string, prop.usage);

		p_script->member_info[name] = pinfo;

		if (prop.exported) {
#ifdef TOOLS_ENABLED
			p_script->exported_members_cache.push_back(pinfo);
#endif

#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
			p_script->exported_members_names.insert(name);
#endif
		}
	}
}

#ifdef TOOLS_ENABLED
void GD_CLR_STDCALL CSharpScript::_add_property_default_values_callback(CSharpScript *p_script, void *p_def_vals, int32_t p_count) {
	GDMonoCache::godotsharp_property_def_val_pair *def_vals = (GDMonoCache::godotsharp_property_def_val_pair *)p_def_vals;

	for (int i = 0; i < p_count; i++) {
		const GDMonoCache::godotsharp_property_def_val_pair &def_val_pair = def_vals[i];

		StringName name = *reinterpret_cast<const StringName *>(&def_val_pair.name);
		Variant value = *reinterpret_cast<const Variant *>(&def_val_pair.value);

		p_script->exported_members_defval_cache[name] = value;
	}
}
#endif

bool CSharpScript::_update_exports(PlaceHolderScriptInstance *p_instance_to_update) {
#ifdef TOOLS_ENABLED
	bool is_editor = Engine::get_singleton()->is_editor_hint();
	if (is_editor) {
		placeholder_fallback_enabled = true; // until proven otherwise
	}
#endif
	if (!valid) {
		return false;
	}

	bool changed = false;

#ifdef TOOLS_ENABLED
	if (exports_invalidated)
#endif
	{
#ifdef TOOLS_ENABLED
		exports_invalidated = false;
#endif

		changed = true;

		member_info.clear();

#ifdef TOOLS_ENABLED
		exported_members_cache.clear();
		exported_members_defval_cache.clear();
#endif

		if (GDMonoCache::godot_api_cache_updated) {
			GDMonoCache::managed_callbacks.ScriptManagerBridge_GetPropertyInfoList(this, &_add_property_info_list_callback);

#ifdef TOOLS_ENABLED
			GDMonoCache::managed_callbacks.ScriptManagerBridge_GetPropertyDefaultValues(this, &_add_property_default_values_callback);
#endif
		}
	}

#ifdef TOOLS_ENABLED
	if (is_editor) {
		placeholder_fallback_enabled = false;

		if ((changed || p_instance_to_update) && placeholders.size()) {
			// Update placeholders if any
			HashMap<StringName, Variant> values;
			List<PropertyInfo> propnames;
			_update_exports_values(values, propnames);

			if (changed) {
				for (PlaceHolderScriptInstance *instance : placeholders) {
					instance->update(propnames, values);
				}
			} else {
				p_instance_to_update->update(propnames, values);
			}
		} else if (placeholders.size()) {
			uint64_t script_modified_time = FileAccess::get_modified_time(get_path());
			uint64_t last_valid_build_time = GDMono::get_singleton()->get_project_assembly_modified_time();
			if (script_modified_time > last_valid_build_time) {
				for (PlaceHolderScriptInstance *instance : placeholders) {
					Object *owner = instance->get_owner();
					if (owner->get_script_instance() == instance) {
						owner->notify_property_list_changed();
					}
				}
			}
		}
	}
#endif

	return changed;
}

bool CSharpScript::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == SNAME("script/source")) {
		r_ret = get_source_code();
		return true;
	}

	return false;
}

bool CSharpScript::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == SNAME("script/source")) {
		set_source_code(p_value);
		reload();
		return true;
	}

	return false;
}

void CSharpScript::_get_property_list(List<PropertyInfo> *p_properties) const {
	p_properties->push_back(PropertyInfo(Variant::STRING, SNAME("script/source"), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
}

void CSharpScript::_bind_methods() {
	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "new", &CSharpScript::_new, MethodInfo("new"));
}

void CSharpScript::reload_registered_script(Ref<CSharpScript> p_script) {
	// IMPORTANT:
	// This method must be called only after the CSharpScript and its associated type
	// have been added to the script bridge map in the ScriptManagerBridge C# class.
	// Other than that, it's the same as `CSharpScript::reload`.

	// This method should not fail, only assertions allowed.

	// Unlike `reload`, we print an error rather than silently returning,
	// as we can assert this won't be called a second time until invalidated.
	ERR_FAIL_COND(!p_script->reload_invalidated);

	p_script->valid = true;
	p_script->reload_invalidated = false;

	update_script_class_info(p_script);

	p_script->_update_exports();

#ifdef TOOLS_ENABLED
	// If the EditorFileSystem singleton is available, update the file;
	// otherwise, the file will be updated when the singleton becomes available.
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	if (efs && !p_script->get_path().is_empty()) {
		efs->update_file(p_script->get_path());
	}
#endif
}

// Extract information about the script using the mono class.
void CSharpScript::update_script_class_info(Ref<CSharpScript> p_script) {
	TypeInfo type_info;

	// TODO: Use GDExtension godot_dictionary
	Array methods_array;
	methods_array.~Array();
	Dictionary rpc_functions_dict;
	rpc_functions_dict.~Dictionary();
	Dictionary signals_dict;
	signals_dict.~Dictionary();

	Ref<CSharpScript> base_script;
	GDMonoCache::managed_callbacks.ScriptManagerBridge_UpdateScriptClassInfo(
			p_script.ptr(), &type_info,
			&methods_array, &rpc_functions_dict, &signals_dict, &base_script);

	p_script->type_info = type_info;

	p_script->rpc_config.clear();
	p_script->rpc_config = rpc_functions_dict;

	// Methods

	p_script->methods.clear();

	p_script->methods.resize(methods_array.size());
	int push_index = 0;

	for (int i = 0; i < methods_array.size(); i++) {
		Dictionary method_info_dict = methods_array[i];

		StringName name = method_info_dict["name"];

		MethodInfo mi;
		mi.name = name;

		mi.return_val = PropertyInfo::from_dict(method_info_dict["return_val"]);

		Array params = method_info_dict["params"];

		for (int j = 0; j < params.size(); j++) {
			Dictionary param = params[j];

			Variant::Type param_type = (Variant::Type)(int)param["type"];
			PropertyInfo arg_info = PropertyInfo(param_type, (String)param["name"]);
			arg_info.usage = (uint32_t)param["usage"];
			if (param.has("class_name")) {
				arg_info.class_name = (StringName)param["class_name"];
			}
			mi.arguments.push_back(arg_info);
		}

		mi.flags = (uint32_t)method_info_dict["flags"];

		p_script->methods.set(push_index++, CSharpMethodInfo{ name, mi });
	}

	// Event signals

	// Performance is not critical here as this will be replaced with source generators.

	p_script->event_signals.clear();

	// Sigh... can't we just have capacity?
	p_script->event_signals.resize(signals_dict.size());
	push_index = 0;

	for (const Variant *s = signals_dict.next(nullptr); s != nullptr; s = signals_dict.next(s)) {
		StringName name = *s;

		MethodInfo mi;
		mi.name = name;

		Array params = signals_dict[*s];

		for (int i = 0; i < params.size(); i++) {
			Dictionary param = params[i];

			Variant::Type param_type = (Variant::Type)(int)param["type"];
			PropertyInfo arg_info = PropertyInfo(param_type, (String)param["name"]);
			arg_info.usage = (uint32_t)param["usage"];
			if (param.has("class_name")) {
				arg_info.class_name = (StringName)param["class_name"];
			}
			mi.arguments.push_back(arg_info);
		}

		p_script->event_signals.set(push_index++, EventSignalInfo{ name, mi });
	}

	p_script->base_script = base_script;
}

bool CSharpScript::can_instantiate() const {
#ifdef TOOLS_ENABLED
	bool extra_cond = type_info.is_tool || ScriptServer::is_scripting_enabled();
#else
	bool extra_cond = true;
#endif

	// FIXME Need to think this through better.
	// For tool scripts, this will never fire if the class is not found. That's because we
	// don't know if it's a tool script if we can't find the class to access the attributes.
	if (extra_cond && !valid) {
		ERR_FAIL_V_MSG(false, "Cannot instantiate C# script because the associated class could not be found. Script: '" + get_path() + "'. Make sure the script exists and contains a class definition with a name that matches the filename of the script exactly (it's case-sensitive).");
	}

	return valid && type_info.can_instantiate() && extra_cond;
}

StringName CSharpScript::get_instance_base_type() const {
	return type_info.native_base_name;
}

CSharpInstance *CSharpScript::_create_instance(const Variant **p_args, int p_argcount, Object *p_owner, bool p_is_ref_counted, Callable::CallError &r_error) {
	ERR_FAIL_COND_V_MSG(!type_info.can_instantiate(), nullptr, "Cannot instantiate C# script. Script: '" + get_path() + "'.");

	/* STEP 1, CREATE */

	Ref<RefCounted> ref;
	if (p_is_ref_counted) {
		// Hold it alive. Important if we have to dispose a script instance binding before creating the CSharpInstance.
		ref = Ref<RefCounted>(static_cast<RefCounted *>(p_owner));
	}

	// If the object had a script instance binding, dispose it before adding the CSharpInstance
	if (CSharpLanguage::has_instance_binding(p_owner)) {
		void *data = CSharpLanguage::get_existing_instance_binding(p_owner);
		CRASH_COND(data == nullptr);

		CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)data)->get();
		if (script_binding.inited && !script_binding.gchandle.is_released()) {
			GDMonoCache::managed_callbacks.CSharpInstanceBridge_CallDispose(
					script_binding.gchandle.get_intptr(), /* okIfNull */ true);

			script_binding.gchandle.release(); // Just in case
			script_binding.inited = false;
		}
	}

	CSharpInstance *instance = memnew(CSharpInstance(Ref<CSharpScript>(this)));
	instance->base_ref_counted = p_is_ref_counted;
	instance->owner = p_owner;
	instance->owner->set_script_instance(instance);

	/* STEP 2, INITIALIZE AND CONSTRUCT */

	bool ok = GDMonoCache::managed_callbacks.ScriptManagerBridge_CreateManagedForGodotObjectScriptInstance(
			this, p_owner, p_args, p_argcount);

	if (!ok) {
		// Important to clear this before destroying the script instance here
		instance->script = Ref<CSharpScript>();
		p_owner->set_script_instance(nullptr);
		instance->owner = nullptr;

		return nullptr;
	}

	CRASH_COND(instance->gchandle.is_released());

	/* STEP 3, PARTY */

	//@TODO make thread safe
	return instance;
}

Variant CSharpScript::_new(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (!valid) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	r_error.error = Callable::CallError::CALL_OK;

	StringName native_name;
	GDMonoCache::managed_callbacks.ScriptManagerBridge_GetScriptNativeName(this, &native_name);

	ERR_FAIL_COND_V(native_name == StringName(), Variant());

	Object *owner = ClassDB::instantiate(native_name);

	Ref<RefCounted> ref;
	RefCounted *r = Object::cast_to<RefCounted>(owner);
	if (r) {
		ref = Ref<RefCounted>(r);
	}

	CSharpInstance *instance = _create_instance(p_args, p_argcount, owner, r != nullptr, r_error);
	if (!instance) {
		if (ref.is_null()) {
			memdelete(owner); // no owner, sorry
		}
		return Variant();
	}

	if (ref.is_valid()) {
		return ref;
	} else {
		return owner;
	}
}

ScriptInstance *CSharpScript::instance_create(Object *p_this) {
#ifdef DEBUG_ENABLED
	CRASH_COND(!valid);
#endif

	StringName native_name;
	GDMonoCache::managed_callbacks.ScriptManagerBridge_GetScriptNativeName(this, &native_name);

	ERR_FAIL_COND_V(native_name == StringName(), nullptr);

	if (!ClassDB::is_parent_class(p_this->get_class_name(), native_name)) {
		if (EngineDebugger::is_active()) {
			CSharpLanguage::get_singleton()->debug_break_parse(get_path(), 0,
					"Script inherits from native type '" + String(native_name) +
							"', so it can't be assigned to an object of type: '" + p_this->get_class() + "'");
		}
		ERR_FAIL_V_MSG(nullptr, "Script inherits from native type '" + String(native_name) + "', so it can't be assigned to an object of type: '" + p_this->get_class() + "'.");
	}

	Callable::CallError unchecked_error;
	return _create_instance(nullptr, 0, p_this, Object::cast_to<RefCounted>(p_this) != nullptr, unchecked_error);
}

PlaceHolderScriptInstance *CSharpScript::placeholder_instance_create(Object *p_this) {
#ifdef TOOLS_ENABLED
	PlaceHolderScriptInstance *si = memnew(PlaceHolderScriptInstance(CSharpLanguage::get_singleton(), Ref<Script>(this), p_this));
	placeholders.insert(si);
	_update_exports(si);
	return si;
#else
	return nullptr;
#endif
}

bool CSharpScript::instance_has(const Object *p_this) const {
	MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
	return instances.has((Object *)p_this);
}

bool CSharpScript::has_source_code() const {
	return !source.is_empty();
}

String CSharpScript::get_source_code() const {
	return source;
}

void CSharpScript::set_source_code(const String &p_code) {
	if (source == p_code) {
		return;
	}
	source = p_code;
#ifdef TOOLS_ENABLED
	source_changed_cache = true;
#endif
}

void CSharpScript::get_script_method_list(List<MethodInfo> *p_list) const {
	if (!valid) {
		return;
	}

	const CSharpScript *top = this;
	while (top != nullptr) {
		for (const CSharpMethodInfo &E : top->methods) {
			p_list->push_back(E.method_info);
		}

		top = top->base_script.ptr();
	}
}

bool CSharpScript::has_method(const StringName &p_method) const {
	if (!valid) {
		return false;
	}

	for (const CSharpMethodInfo &E : methods) {
		if (E.name == p_method) {
			return true;
		}
	}

	return false;
}

int CSharpScript::get_script_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	if (!valid) {
		if (r_is_valid) {
			*r_is_valid = false;
		}
		return 0;
	}

	for (const CSharpMethodInfo &E : methods) {
		if (E.name == p_method) {
			if (r_is_valid) {
				*r_is_valid = true;
			}
			return E.method_info.arguments.size();
		}
	}

	if (r_is_valid) {
		*r_is_valid = false;
	}
	return 0;
}

MethodInfo CSharpScript::get_method_info(const StringName &p_method) const {
	if (!valid) {
		return MethodInfo();
	}

	MethodInfo mi;
	for (const CSharpMethodInfo &E : methods) {
		if (E.name == p_method) {
			if (mi.name == p_method) {
				// We already found a method with the same name before so
				// that means this method has overloads, the best we can do
				// is return an empty MethodInfo.
				return MethodInfo();
			}
			mi = E.method_info;
		}
	}

	return mi;
}

Variant CSharpScript::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (valid) {
		Variant ret;
		bool ok = GDMonoCache::managed_callbacks.ScriptManagerBridge_CallStatic(this, &p_method, p_args, p_argcount, &r_error, &ret);
		if (ok) {
			return ret;
		}
	}

	return Script::callp(p_method, p_args, p_argcount, r_error);
}

Error CSharpScript::reload(bool p_keep_state) {
	if (!reload_invalidated) {
		return OK;
	}

	// In the case of C#, reload doesn't really do any script reloading.
	// That's done separately via domain reloading.
	reload_invalidated = false;

	String script_path = get_path();

	valid = GDMonoCache::managed_callbacks.ScriptManagerBridge_AddScriptBridge(this, &script_path);

	if (valid) {
#ifdef DEBUG_ENABLED
		print_verbose("Found class for script " + get_path());
#endif

		update_script_class_info(this);

		_update_exports();

#ifdef TOOLS_ENABLED
		// If the EditorFileSystem singleton is available, update the file;
		// otherwise, the file will be updated when the singleton becomes available.
		EditorFileSystem *efs = EditorFileSystem::get_singleton();
		if (efs) {
			efs->update_file(script_path);
		}
#endif
	}

	return OK;
}

ScriptLanguage *CSharpScript::get_language() const {
	return CSharpLanguage::get_singleton();
}

bool CSharpScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
#ifdef TOOLS_ENABLED

	HashMap<StringName, Variant>::ConstIterator E = exported_members_defval_cache.find(p_property);
	if (E) {
		r_value = E->value;
		return true;
	}

	if (base_script.is_valid()) {
		return base_script->get_property_default_value(p_property, r_value);
	}

#endif
	return false;
}

void CSharpScript::update_exports() {
#ifdef TOOLS_ENABLED
	_update_exports();
#endif
}

bool CSharpScript::has_script_signal(const StringName &p_signal) const {
	if (!valid) {
		return false;
	}

	if (!GDMonoCache::godot_api_cache_updated) {
		return false;
	}

	for (const EventSignalInfo &signal : event_signals) {
		if (signal.name == p_signal) {
			return true;
		}
	}

	if (base_script.is_valid()) {
		return base_script->has_script_signal(p_signal);
	}

	return false;
}

void CSharpScript::_get_script_signal_list(List<MethodInfo> *r_signals, bool p_include_base) const {
	if (!valid) {
		return;
	}

	for (const EventSignalInfo &signal : event_signals) {
		r_signals->push_back(signal.method_info);
	}

	if (!p_include_base) {
		return;
	}

	if (base_script.is_valid()) {
		base_script->get_script_signal_list(r_signals);
	}
}

void CSharpScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	_get_script_signal_list(r_signals, true);
}

bool CSharpScript::inherits_script(const Ref<Script> &p_script) const {
	Ref<CSharpScript> cs = p_script;
	if (cs.is_null()) {
		return false;
	}

	if (!valid || !cs->valid) {
		return false;
	}

	if (!GDMonoCache::godot_api_cache_updated) {
		return false;
	}

	return GDMonoCache::managed_callbacks.ScriptManagerBridge_ScriptIsOrInherits(this, cs.ptr());
}

Ref<Script> CSharpScript::get_base_script() const {
	return base_script;
}

StringName CSharpScript::get_global_name() const {
	return type_info.is_global_class ? StringName(type_info.class_name) : StringName();
}

void CSharpScript::get_script_property_list(List<PropertyInfo> *r_list) const {
#ifdef TOOLS_ENABLED
	const CSharpScript *top = this;
	while (top != nullptr) {
		for (const PropertyInfo &E : top->exported_members_cache) {
			r_list->push_back(E);
		}

		top = top->base_script.ptr();
	}
#else
	const CSharpScript *top = this;
	while (top != nullptr) {
		List<PropertyInfo> props;

		for (const KeyValue<StringName, PropertyInfo> &E : top->member_info) {
			props.push_front(E.value);
		}

		for (const PropertyInfo &prop : props) {
			r_list->push_back(prop);
		}

		top = top->base_script.ptr();
	}
#endif
}

int CSharpScript::get_member_line(const StringName &p_member) const {
	// TODO omnisharp
	return -1;
}

Variant CSharpScript::get_rpc_config() const {
	return rpc_config;
}

Error CSharpScript::load_source_code(const String &p_path) {
	Error ferr = read_all_file_utf8(p_path, source);

	ERR_FAIL_COND_V_MSG(ferr != OK, ferr,
			ferr == ERR_INVALID_DATA
					? "Script '" + p_path + "' contains invalid unicode (UTF-8), so it was not loaded."
											" Please ensure that scripts are saved in valid UTF-8 unicode."
					: "Failed to read file: '" + p_path + "'.");

#ifdef TOOLS_ENABLED
	source_changed_cache = true;
#endif

	return OK;
}

void CSharpScript::_clear() {
	type_info = TypeInfo();
	valid = false;
	reload_invalidated = true;
}

CSharpScript::CSharpScript() {
	_clear();

#ifdef DEBUG_ENABLED
	{
		MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
		CSharpLanguage::get_singleton()->script_list.add(&script_list);
	}
#endif
}

CSharpScript::~CSharpScript() {
#ifdef DEBUG_ENABLED
	{
		MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
		CSharpLanguage::get_singleton()->script_list.remove(&script_list);
	}
#endif

	if (GDMonoCache::godot_api_cache_updated) {
		GDMonoCache::managed_callbacks.ScriptManagerBridge_RemoveScriptBridge(this);
	}
}

void CSharpScript::get_members(HashSet<StringName> *p_members) {
#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
	if (p_members) {
		for (const StringName &member_name : exported_members_names) {
			p_members->insert(member_name);
		}
	}
#endif
}

/*************** RESOURCE ***************/

Ref<Resource> ResourceFormatLoaderCSharpScript::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	// TODO ignore anything inside bin/ and obj/ in tools builds?

	String real_path = p_path;
	if (p_path.begins_with("csharp://")) {
		// This is a virtual path used by generic types, extract the real path.
		real_path = "res://" + p_path.trim_prefix("csharp://");
		real_path = real_path.substr(0, real_path.rfind_char(':'));
	}

	Ref<CSharpScript> scr;

	if (GDMonoCache::godot_api_cache_updated) {
		GDMonoCache::managed_callbacks.ScriptManagerBridge_GetOrCreateScriptBridgeForPath(&p_path, &scr);
		ERR_FAIL_COND_V_MSG(scr.is_null(), Ref<Resource>(), "Could not create C# script '" + real_path + "'.");
	} else {
		scr.instantiate();
	}

#if defined(DEBUG_ENABLED) || defined(TOOLS_ENABLED)
	Error err = scr->load_source_code(real_path);
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), "Cannot load C# script file '" + real_path + "'.");
#endif

	// Only one instance of a C# script is allowed to exist.
	ERR_FAIL_COND_V_MSG(!scr->get_path().is_empty() && scr->get_path() != p_original_path, Ref<Resource>(),
			"The C# script path is different from the path it was registered in the C# dictionary.");

	Ref<Resource> existing = ResourceCache::get_ref(p_path);
	switch (p_cache_mode) {
		case ResourceFormatLoader::CACHE_MODE_IGNORE:
		case ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP:
			break;
		case ResourceFormatLoader::CACHE_MODE_REUSE:
			if (existing.is_null()) {
				scr->set_path(p_original_path);
			} else {
				scr = existing;
			}
			break;
		case ResourceFormatLoader::CACHE_MODE_REPLACE:
		case ResourceFormatLoader::CACHE_MODE_REPLACE_DEEP:
			scr->set_path(p_original_path, true);
			break;
	}

	scr->reload();

	if (r_error) {
		*r_error = OK;
	}

	return scr;
}

void ResourceFormatLoaderCSharpScript::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("cs");
}

bool ResourceFormatLoaderCSharpScript::handles_type(const String &p_type) const {
	return p_type == "Script" || p_type == CSharpLanguage::get_singleton()->get_type();
}

String ResourceFormatLoaderCSharpScript::get_resource_type(const String &p_path) const {
	return p_path.get_extension().to_lower() == "cs" ? CSharpLanguage::get_singleton()->get_type() : "";
}

Error ResourceFormatSaverCSharpScript::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<CSharpScript> sqscr = p_resource;
	ERR_FAIL_COND_V(sqscr.is_null(), ERR_INVALID_PARAMETER);

	String source = sqscr->get_source_code();

#ifdef TOOLS_ENABLED
	if (!FileAccess::exists(p_path)) {
		// The file does not yet exist, let's assume the user just created this script. In such
		// cases we need to check whether the solution and csproj were already created or not.
		if (!_create_project_solution_if_needed()) {
			ERR_PRINT("C# project could not be created; cannot add file: '" + p_path + "'.");
		}
	}
#endif

	{
		Error err;
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save C# script file '" + p_path + "'.");

		file->store_string(source);

		if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
			return ERR_CANT_CREATE;
		}
	}

#ifdef TOOLS_ENABLED
	if (ScriptServer::is_reload_scripts_on_save_enabled()) {
		CSharpLanguage::get_singleton()->reload_tool_script(p_resource, false);
	}
#endif

	return OK;
}

void ResourceFormatSaverCSharpScript::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<CSharpScript>(p_resource.ptr())) {
		p_extensions->push_back("cs");
	}
}

bool ResourceFormatSaverCSharpScript::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<CSharpScript>(p_resource.ptr()) != nullptr;
}
