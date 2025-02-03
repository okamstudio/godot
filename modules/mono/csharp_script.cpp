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

#include <mono/metadata/threads.h>

#include "core/io/json.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/project_settings.h"

#ifdef TOOLS_ENABLED
#include "core/os/keyboard.h"
#include "editor/bindings_generator.h"
#include "editor/csharp_project.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/node_dock.h"
#endif

#ifdef DEBUG_METHODS_ENABLED
#include "class_db_api_json.h"
#endif

#include "editor/editor_internal_calls.h"
#include "godotsharp_dirs.h"
#include "mono_gd/gd_mono_cache.h"
#include "mono_gd/gd_mono_class.h"
#include "mono_gd/gd_mono_marshal.h"
#include "mono_gd/gd_mono_utils.h"
#include "signal_awaiter_utils.h"
#include "utils/macros.h"
#include "utils/path_utils.h"
#include "utils/string_utils.h"
#include "utils/thread_local.h"

#define CACHED_STRING_NAME(m_var) (CSharpLanguage::get_singleton()->get_string_names().m_var)

#ifdef TOOLS_ENABLED
static bool _create_project_solution_if_needed() {
	String sln_path = GodotSharpDirs::get_project_sln_path();
	String csproj_path = GodotSharpDirs::get_project_csproj_path();

	if (!FileAccess::exists(sln_path) || !FileAccess::exists(csproj_path)) {
		// A solution does not yet exist, create a new one

		CRASH_COND(CSharpLanguage::get_singleton()->get_godotsharp_editor() == NULL);
		return CSharpLanguage::get_singleton()->get_godotsharp_editor()->call("CreateProjectSolution");
	}

	return true;
}
#endif

CSharpLanguage *CSharpLanguage::singleton = NULL;

String CSharpLanguage::get_name() const {
	return "C#";
}

String CSharpLanguage::get_type() const {
	return "CSharpScript";
}

String CSharpLanguage::get_extension() const {
	return "cs";
}

Error CSharpLanguage::execute_file(const String &p_path) {
	// ??
	return OK;
}

void CSharpLanguage::init() {
#ifdef DEBUG_METHODS_ENABLED
	if (OS::get_singleton()->get_cmdline_args().find("--class-db-json")) {
		class_db_api_to_json("user://class_db_api.json", ClassDB::API_CORE);
#ifdef TOOLS_ENABLED
		class_db_api_to_json("user://class_db_api_editor.json", ClassDB::API_EDITOR);
#endif
	}
#endif

	gdmono = memnew(GDMono);
	gdmono->initialize();

#if defined(TOOLS_ENABLED) && defined(DEBUG_METHODS_ENABLED)
	// Generate bindings here, before loading assemblies. 'initialize_load_assemblies' aborts
	// the applications if the api assemblies or the main tools assembly is missing, but this
	// is not a problem for BindingsGenerator as it only needs the tools project editor assembly.
	List<String> cmdline_args = OS::get_singleton()->get_cmdline_args();
	BindingsGenerator::handle_cmdline_args(cmdline_args);
#endif

#ifndef MONO_GLUE_ENABLED
	print_line("Run this binary with '--generate-mono-glue path/to/modules/mono/glue'");
#endif

	if (gdmono->is_runtime_initialized())
		gdmono->initialize_load_assemblies();

#ifdef TOOLS_ENABLED
	EditorNode::add_init_callback(&_editor_init_callback);
#endif
}

void CSharpLanguage::finish() {
	finalizing = true;

	// Make sure all script binding gchandles are released before finalizing GDMono
	for (Map<Object *, CSharpScriptBinding>::Element *E = script_bindings.front(); E; E = E->next()) {
		CSharpScriptBinding &script_binding = E->value();

		if (script_binding.gchandle.is_valid()) {
			script_binding.gchandle->release();
			script_binding.inited = false;
		}
	}

	if (gdmono) {
		memdelete(gdmono);
		gdmono = NULL;
	}

	// Clear here, after finalizing all domains to make sure there is nothing else referencing the elements.
	script_bindings.clear();

#ifdef DEBUG_ENABLED
	for (Map<ObjectID, int>::Element *E = unsafe_object_references.front(); E; E = E->next()) {
		const ObjectID &id = E->key();
		Object *obj = ObjectDB::get_instance(id);

		if (obj) {
			ERR_PRINT("Leaked unsafe reference to object: " + obj->to_string());
		} else {
			ERR_PRINT("Leaked unsafe reference to deleted object: " + itos(id));
		}
	}
#endif

	finalizing = false;
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
		0
	};

	const char **w = _reserved_words;

	while (*w) {
		p_words->push_back(*w);
		w++;
	}
}

bool CSharpLanguage::is_control_flow_keyword(String p_keyword) const {
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

void CSharpLanguage::get_string_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("' '"); // character literal
	p_delimiters->push_back("\" \""); // regular string literal
	// Verbatim string literals (`@" "`) don't render correctly, so don't highlight them.
	// Generic string highlighting suffices as a workaround for now.
}

static String get_base_class_name(const String &p_base_class_name, const String p_class_name) {
	String base_class = p_base_class_name;
	if (p_class_name == base_class) {
		base_class = "Godot." + base_class;
	}
	return base_class;
}

Ref<Script> CSharpLanguage::get_template(const String &p_class_name, const String &p_base_class_name) const {
	String script_template = "using " BINDINGS_NAMESPACE ";\n"
							 "using System;\n"
							 "\n"
							 "public class %CLASS% : %BASE%\n"
							 "{\n"
							 "    // Declare member variables here. Examples:\n"
							 "    // private int a = 2;\n"
							 "    // private string b = \"text\";\n"
							 "\n"
							 "    // Called when the node enters the scene tree for the first time.\n"
							 "    public override void _Ready()\n"
							 "    {\n"
							 "        \n"
							 "    }\n"
							 "\n"
							 "//  // Called every frame. 'delta' is the elapsed time since the previous frame.\n"
							 "//  public override void _Process(float delta)\n"
							 "//  {\n"
							 "//      \n"
							 "//  }\n"
							 "}\n";

	// Replaces all spaces in p_class_name with underscores to prevent
	// invalid C# Script templates from being generated when the object name
	// has spaces in it.
	String class_name_no_spaces = p_class_name.replace(" ", "_");
	String base_class_name = get_base_class_name(p_base_class_name, class_name_no_spaces);
	script_template = script_template.replace("%BASE%", base_class_name)
							  .replace("%CLASS%", class_name_no_spaces);

	Ref<CSharpScript> script;
	script.instance();
	script->set_source_code(script_template);
	script->set_name(class_name_no_spaces);

	return script;
}

bool CSharpLanguage::is_using_templates() {
	return true;
}

void CSharpLanguage::make_template(const String &p_class_name, const String &p_base_class_name, Ref<Script> &p_script) {
	String src = p_script->get_source_code();
	String class_name_no_spaces = p_class_name.replace(" ", "_");
	String base_class_name = get_base_class_name(p_base_class_name, class_name_no_spaces);
	src = src.replace("%BASE%", base_class_name)
				  .replace("%CLASS%", class_name_no_spaces)
				  .replace("%TS%", _get_indentation());
	p_script->set_source_code(src);
}

String CSharpLanguage::validate_path(const String &p_path) const {
	String class_name = p_path.get_file().get_basename();
	List<String> keywords;
	get_reserved_words(&keywords);
	if (keywords.find(class_name)) {
		return TTR("Class name can't be a reserved keyword");
	}
	return "";
}

Script *CSharpLanguage::create_script() const {
	return memnew(CSharpScript);
}

bool CSharpLanguage::has_named_classes() const {
	return false;
}

bool CSharpLanguage::supports_builtin_mode() const {
	return false;
}

#ifdef TOOLS_ENABLED
static String variant_type_to_managed_name(const String &p_var_type_name) {
	if (p_var_type_name.empty())
		return "object";

	if (!ClassDB::class_exists(p_var_type_name)) {
		return p_var_type_name;
	}

	if (p_var_type_name == Variant::get_type_name(Variant::OBJECT))
		return "Godot.Object";

	if (p_var_type_name == Variant::get_type_name(Variant::REAL)) {
#ifdef REAL_T_IS_DOUBLE
		return "double";
#else
		return "float";
#endif
	}

	if (p_var_type_name == Variant::get_type_name(Variant::STRING))
		return "string"; // I prefer this one >:[

	if (p_var_type_name == Variant::get_type_name(Variant::DICTIONARY))
		return "Collections.Dictionary";

	if (p_var_type_name == Variant::get_type_name(Variant::ARRAY))
		return "Collections.Array";

	if (p_var_type_name == Variant::get_type_name(Variant::POOL_BYTE_ARRAY))
		return "byte[]";
	if (p_var_type_name == Variant::get_type_name(Variant::POOL_INT_ARRAY))
		return "int[]";
	if (p_var_type_name == Variant::get_type_name(Variant::POOL_REAL_ARRAY)) {
#ifdef REAL_T_IS_DOUBLE
		return "double[]";
#else
		return "float[]";
#endif
	}
	if (p_var_type_name == Variant::get_type_name(Variant::POOL_STRING_ARRAY))
		return "string[]";
	if (p_var_type_name == Variant::get_type_name(Variant::POOL_VECTOR2_ARRAY))
		return "Vector2[]";
	if (p_var_type_name == Variant::get_type_name(Variant::POOL_VECTOR3_ARRAY))
		return "Vector3[]";
	if (p_var_type_name == Variant::get_type_name(Variant::POOL_COLOR_ARRAY))
		return "Color[]";

	Variant::Type var_types[] = {
		Variant::BOOL,
		Variant::INT,
		Variant::VECTOR2,
		Variant::RECT2,
		Variant::VECTOR3,
		Variant::TRANSFORM2D,
		Variant::PLANE,
		Variant::QUAT,
		Variant::AABB,
		Variant::BASIS,
		Variant::TRANSFORM,
		Variant::COLOR,
		Variant::NODE_PATH,
		Variant::_RID
	};

	for (unsigned int i = 0; i < sizeof(var_types) / sizeof(Variant::Type); i++) {
		if (p_var_type_name == Variant::get_type_name(var_types[i]))
			return p_var_type_name;
	}

	return "object";
}

String CSharpLanguage::make_function(const String &, const String &p_name, const PoolStringArray &p_args) const {
	// FIXME
	// - Due to Godot's API limitation this just appends the function to the end of the file
	// - Use fully qualified name if there is ambiguity
	String s = "private void " + p_name + "(";
	for (int i = 0; i < p_args.size(); i++) {
		const String &arg = p_args[i];

		if (i > 0)
			s += ", ";

		s += variant_type_to_managed_name(arg.get_slice(":", 1)) + " " + escape_csharp_keyword(arg.get_slice(":", 0));
	}
	s += ")\n{\n    // Replace with function body.\n}\n";

	return s;
}
#else
String CSharpLanguage::make_function(const String &, const String &, const PoolStringArray &) const {
	return String();
}
#endif

String CSharpLanguage::_get_indentation() const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		bool use_space_indentation = EDITOR_GET("text_editor/indent/type");

		if (use_space_indentation) {
			int indent_size = EDITOR_GET("text_editor/indent/size");

			String space_indent = "";
			for (int i = 0; i < indent_size; i++) {
				space_indent += " ";
			}
			return space_indent;
		}
	}
#endif
	return "\t";
}

String CSharpLanguage::debug_get_error() const {
	return _debug_error;
}

int CSharpLanguage::debug_get_stack_level_count() const {
	if (_debug_parse_err_line >= 0)
		return 1;

	// TODO: StackTrace
	return 1;
}

int CSharpLanguage::debug_get_stack_level_line(int p_level) const {
	if (_debug_parse_err_line >= 0)
		return _debug_parse_err_line;

	// TODO: StackTrace
	return 1;
}

String CSharpLanguage::debug_get_stack_level_function(int p_level) const {
	if (_debug_parse_err_line >= 0)
		return String();

	// TODO: StackTrace
	return String();
}

String CSharpLanguage::debug_get_stack_level_source(int p_level) const {
	if (_debug_parse_err_line >= 0)
		return _debug_parse_err_file;

	// TODO: StackTrace
	return String();
}

Vector<ScriptLanguage::StackInfo> CSharpLanguage::debug_get_current_stack_info() {
#ifdef DEBUG_ENABLED
	_TLS_RECURSION_GUARD_V_(Vector<StackInfo>());
	GD_MONO_SCOPE_THREAD_ATTACH;

	if (!gdmono || !gdmono->is_runtime_initialized() || !GDMono::get_singleton()->get_core_api_assembly() || !GDMonoCache::cached_data.corlib_cache_updated)
		return Vector<StackInfo>();

	MonoObject *stack_trace = mono_object_new(mono_domain_get(), CACHED_CLASS(System_Diagnostics_StackTrace)->get_mono_ptr());

	MonoBoolean need_file_info = true;
	void *ctor_args[1] = { &need_file_info };

	CACHED_METHOD(System_Diagnostics_StackTrace, ctor_bool)->invoke_raw(stack_trace, ctor_args);

	Vector<StackInfo> si;
	si = stack_trace_get_info(stack_trace);

	return si;
#else
	return Vector<StackInfo>();
#endif
}

#ifdef DEBUG_ENABLED
Vector<ScriptLanguage::StackInfo> CSharpLanguage::stack_trace_get_info(MonoObject *p_stack_trace) {
	_TLS_RECURSION_GUARD_V_(Vector<StackInfo>());
	GD_MONO_SCOPE_THREAD_ATTACH;

	MonoException *exc = NULL;

	MonoArray *frames = CACHED_METHOD_THUNK(System_Diagnostics_StackTrace, GetFrames).invoke(p_stack_trace, &exc);

	if (exc) {
		GDMonoUtils::debug_print_unhandled_exception(exc);
		return Vector<StackInfo>();
	}

	int frame_count = mono_array_length(frames);

	if (frame_count <= 0)
		return Vector<StackInfo>();

	Vector<StackInfo> si;
	si.resize(frame_count);

	for (int i = 0; i < frame_count; i++) {
		StackInfo &sif = si.write[i];
		MonoObject *frame = mono_array_get(frames, MonoObject *, i);

		MonoString *file_name;
		int file_line_num;
		MonoString *method_decl;
		CACHED_METHOD_THUNK(DebuggingUtils, GetStackFrameInfo).invoke(frame, &file_name, &file_line_num, &method_decl, &exc);

		if (exc) {
			GDMonoUtils::debug_print_unhandled_exception(exc);
			return Vector<StackInfo>();
		}

		// TODO
		// what if the StackFrame method is null (method_decl is empty). should we skip this frame?
		// can reproduce with a MissingMethodException on internal calls

		sif.file = GDMonoMarshal::mono_string_to_godot(file_name);
		sif.line = file_line_num;
		sif.func = GDMonoMarshal::mono_string_to_godot(method_decl);
	}

	return si;
}
#endif

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
	Map<ObjectID, int>::Element *elem = unsafe_object_references.find(id);
	ERR_FAIL_NULL(elem);
	if (--elem->value() == 0)
		unsafe_object_references.erase(elem);
#endif
}

void CSharpLanguage::frame() {
	if (gdmono && gdmono->is_runtime_initialized() && gdmono->get_core_api_assembly() != NULL) {
		const Ref<MonoGCHandle> &task_scheduler_handle = GDMonoCache::cached_data.task_scheduler_handle;

		if (task_scheduler_handle.is_valid()) {
			MonoObject *task_scheduler = task_scheduler_handle->get_target();

			if (task_scheduler) {
				MonoException *exc = NULL;
				CACHED_METHOD_THUNK(GodotTaskScheduler, Activate).invoke(task_scheduler, &exc);

				if (exc) {
					GDMonoUtils::debug_unhandled_exception(exc);
				}
			}
		}
	}
}

struct CSharpScriptDepSort {
	// must support sorting so inheritance works properly (parent must be reloaded first)
	bool operator()(const Ref<CSharpScript> &A, const Ref<CSharpScript> &B) const {
		if (A == B)
			return false; // shouldn't happen but..
		GDMonoClass *I = B->base;
		while (I) {
			if (I == A->script_class) {
				// A is a base of B
				return true;
			}

			I = I->get_parent_class();
		}

		return false; // not a base
	}
};

void CSharpLanguage::reload_all_scripts() {
#ifdef GD_MONO_HOT_RELOAD
	if (is_assembly_reloading_needed()) {
		GD_MONO_SCOPE_THREAD_ATTACH;
		reload_assemblies(false);
	}
#endif
}

void CSharpLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
	(void)p_script; // UNUSED

	CRASH_COND(!Engine::get_singleton()->is_editor_hint());

#ifdef TOOLS_ENABLED
	get_godotsharp_editor()->get_node(NodePath("HotReloadAssemblyWatcher"))->call("RestartTimer");
#endif

#ifdef GD_MONO_HOT_RELOAD
	if (is_assembly_reloading_needed()) {
		GD_MONO_SCOPE_THREAD_ATTACH;
		reload_assemblies(p_soft_reload);
	}
#endif
}

#ifdef GD_MONO_HOT_RELOAD
bool CSharpLanguage::is_assembly_reloading_needed() {
	if (!gdmono || !gdmono->is_runtime_initialized())
		return false;

	GDMonoAssembly *proj_assembly = gdmono->get_project_assembly();

	String assembly_name = path::get_csharp_project_name();

	assembly_name += ".dll";

	if (proj_assembly) {
		String proj_asm_path = proj_assembly->get_path();

		if (!FileAccess::exists(proj_asm_path)) {
			// Maybe it wasn't loaded from the default path, so check this as well
			proj_asm_path = GodotSharpDirs::get_res_temp_assemblies_dir().plus_file(assembly_name);
			if (!FileAccess::exists(proj_asm_path))
				return false; // No assembly to load
		}

		if (FileAccess::get_modified_time(proj_asm_path) <= proj_assembly->get_modified_time())
			return false; // Already up to date
	} else {
		if (!FileAccess::exists(GodotSharpDirs::get_res_temp_assemblies_dir().plus_file(assembly_name)))
			return false; // No assembly to load
	}

	return true;
}

void CSharpLanguage::reload_assemblies(bool p_soft_reload) {
	if (!gdmono || !gdmono->is_runtime_initialized())
		return;

	// There is no soft reloading with Mono. It's always hard reloading.

	List<Ref<CSharpScript>> scripts;

	{
		MutexLock lock(script_instances_mutex);

		for (SelfList<CSharpScript> *elem = script_list.first(); elem; elem = elem->next()) {
			// Cast to CSharpScript to avoid being erased by accident
			scripts.push_back(Ref<CSharpScript>(elem->self()));
		}
	}

	List<Ref<CSharpScript>> to_reload;

	// We need to keep reference instances alive during reloading
	List<Ref<Reference>> ref_instances;

	for (Map<Object *, CSharpScriptBinding>::Element *E = script_bindings.front(); E; E = E->next()) {
		CSharpScriptBinding &script_binding = E->value();
		Reference *ref = Object::cast_to<Reference>(script_binding.owner);
		if (ref) {
			ref_instances.push_back(Ref<Reference>(ref));
		}
	}

	// As scripts are going to be reloaded, must proceed without locking here

	scripts.sort_custom<CSharpScriptDepSort>(); // Update in inheritance dependency order

	for (List<Ref<CSharpScript>>::Element *E = scripts.front(); E; E = E->next()) {
		Ref<CSharpScript> &script = E->get();
		// If someone removes a script from a node, deletes the script, builds, adds a script to the
		// same node, then builds again, the script might have no path and also no script_class. In
		// that case, we can't (and don't need to) reload it.
		if (script->get_path().empty() && !script->script_class) {
			continue;
		}

		to_reload.push_back(script);

		if (script->get_path().empty()) {
			script->tied_class_name_for_reload = script->script_class->get_name_for_lookup();
			script->tied_class_namespace_for_reload = script->script_class->get_namespace();
		}

		// Script::instances are deleted during managed object disposal, which happens on domain finalize.
		// Only placeholders are kept. Therefore we need to keep a copy before that happens.

		for (Set<Object *>::Element *F = script->instances.front(); F; F = F->next()) {
			Object *obj = F->get();
			script->pending_reload_instances.insert(obj->get_instance_id());

			Reference *ref = Object::cast_to<Reference>(obj);
			if (ref) {
				ref_instances.push_back(Ref<Reference>(ref));
			}
		}

#ifdef TOOLS_ENABLED
		for (Set<PlaceHolderScriptInstance *>::Element *F = script->placeholders.front(); F; F = F->next()) {
			Object *obj = F->get()->get_owner();
			script->pending_reload_instances.insert(obj->get_instance_id());

			Reference *ref = Object::cast_to<Reference>(obj);
			if (ref) {
				ref_instances.push_back(Ref<Reference>(ref));
			}
		}
#endif

		// Save state and remove script from instances
		Map<ObjectID, CSharpScript::StateBackup> &owners_map = script->pending_reload_state;

		for (Set<Object *>::Element *F = script->instances.front(); F; F = F->next()) {
			Object *obj = F->get();

			ERR_CONTINUE(!obj->get_script_instance());

			CSharpInstance *csi = static_cast<CSharpInstance *>(obj->get_script_instance());

			// Call OnBeforeSerialize
			if (csi->script->script_class->implements_interface(CACHED_CLASS(ISerializationListener)))
				obj->get_script_instance()->call_multilevel(string_names.on_before_serialize);

			// Save instance info
			CSharpScript::StateBackup state;

			// TODO: Proper state backup (Not only variants, serialize managed state of scripts)
			csi->get_properties_state_for_reloading(state.properties);

			owners_map[obj->get_instance_id()] = state;
		}
	}

	// After the state of all instances is saved, clear scripts and script instances
	for (List<Ref<CSharpScript>>::Element *E = scripts.front(); E; E = E->next()) {
		Ref<CSharpScript> &script = E->get();

		while (script->instances.front()) {
			Object *obj = script->instances.front()->get();
			obj->set_script(RefPtr()); // Remove script and existing script instances (placeholder are not removed before domain reload)
		}

		script->_clear();
	}

	// Do domain reload
	if (gdmono->reload_scripts_domain() != OK) {
		// Failed to reload the scripts domain
		// Make sure to add the scripts back to their owners before returning
		for (List<Ref<CSharpScript>>::Element *E = to_reload.front(); E; E = E->next()) {
			Ref<CSharpScript> scr = E->get();

			for (const Map<ObjectID, CSharpScript::StateBackup>::Element *F = scr->pending_reload_state.front(); F; F = F->next()) {
				Object *obj = ObjectDB::get_instance(F->key());

				if (!obj)
					continue;

				ObjectID obj_id = obj->get_instance_id();

				// Use a placeholder for now to avoid losing the state when saving a scene

				obj->set_script(scr.get_ref_ptr());

				PlaceHolderScriptInstance *placeholder = scr->placeholder_instance_create(obj);
				obj->set_script_instance(placeholder);

#ifdef TOOLS_ENABLED
				// Even though build didn't fail, this tells the placeholder to keep properties and
				// it allows using property_set_fallback for restoring the state without a valid script.
				scr->placeholder_fallback_enabled = true;
#endif

				// Restore Variant properties state, it will be kept by the placeholder until the next script reloading
				for (List<Pair<StringName, Variant>>::Element *G = scr->pending_reload_state[obj_id].properties.front(); G; G = G->next()) {
					placeholder->property_set_fallback(G->get().first, G->get().second, NULL);
				}

				scr->pending_reload_state.erase(obj_id);
			}
		}

		return;
	}

	List<Ref<CSharpScript>> to_reload_state;

	for (List<Ref<CSharpScript>>::Element *E = to_reload.front(); E; E = E->next()) {
		Ref<CSharpScript> script = E->get();

		if (!script->get_path().empty()) {
#ifdef TOOLS_ENABLED
			script->exports_invalidated = true;
#endif
			script->signals_invalidated = true;

			script->reload(p_soft_reload);
			script->update_exports();

			if (!script->valid) {
				script->pending_reload_instances.clear();
				continue;
			}
		} else {
			const StringName &class_namespace = script->tied_class_namespace_for_reload;
			const StringName &class_name = script->tied_class_name_for_reload;
			GDMonoAssembly *project_assembly = gdmono->get_project_assembly();

			// Search in project and tools assemblies first as those are the most likely to have the class
			GDMonoClass *script_class = (project_assembly ? project_assembly->get_class(class_namespace, class_name) : NULL);

#ifdef TOOLS_ENABLED
			if (!script_class) {
				GDMonoAssembly *tools_assembly = gdmono->get_tools_assembly();
				script_class = (tools_assembly ? tools_assembly->get_class(class_namespace, class_name) : NULL);
			}
#endif

			if (!script_class) {
				script_class = gdmono->get_class(class_namespace, class_name);
			}

			if (!script_class) {
				// The class was removed, can't reload
				script->pending_reload_instances.clear();
				continue;
			}

			bool obj_type = CACHED_CLASS(GodotObject)->is_assignable_from(script_class);
			if (!obj_type) {
				// The class no longer inherits Godot.Object, can't reload
				script->pending_reload_instances.clear();
				continue;
			}

			GDMonoClass *native = GDMonoUtils::get_class_native_base(script_class);

			CSharpScript::initialize_for_managed_type(script, script_class, native);
		}

		String native_name = NATIVE_GDMONOCLASS_NAME(script->native);

		{
			for (Set<ObjectID>::Element *F = script->pending_reload_instances.front(); F; F = F->next()) {
				ObjectID obj_id = F->get();
				Object *obj = ObjectDB::get_instance(obj_id);

				if (!obj) {
					script->pending_reload_state.erase(obj_id);
					continue;
				}

				if (!ClassDB::is_parent_class(obj->get_class_name(), native_name)) {
					// No longer inherits the same compatible type, can't reload
					script->pending_reload_state.erase(obj_id);
					continue;
				}

				ScriptInstance *si = obj->get_script_instance();

#ifdef TOOLS_ENABLED
				if (si) {
					// If the script instance is not null, then it must be a placeholder.
					// Non-placeholder script instances are removed in godot_icall_Object_Disposed.
					CRASH_COND(!si->is_placeholder());

					if (script->is_tool() || ScriptServer::is_scripting_enabled()) {
						// Replace placeholder with a script instance

						CSharpScript::StateBackup &state_backup = script->pending_reload_state[obj_id];

						// Backup placeholder script instance state before replacing it with a script instance
						si->get_property_state(state_backup.properties);

						ScriptInstance *script_instance = script->instance_create(obj);

						if (script_instance) {
							script->placeholders.erase(static_cast<PlaceHolderScriptInstance *>(si));
							obj->set_script_instance(script_instance);
						}
					}

					continue;
				}
#else
				CRASH_COND(si != NULL);
#endif
				// Re-create script instance
				obj->set_script(script.get_ref_ptr()); // will create the script instance as well
			}
		}

		to_reload_state.push_back(script);
	}

	for (List<Ref<CSharpScript>>::Element *E = to_reload_state.front(); E; E = E->next()) {
		Ref<CSharpScript> script = E->get();

		for (Set<ObjectID>::Element *F = script->pending_reload_instances.front(); F; F = F->next()) {
			ObjectID obj_id = F->get();
			Object *obj = ObjectDB::get_instance(obj_id);

			if (!obj) {
				script->pending_reload_state.erase(obj_id);
				continue;
			}

			ERR_CONTINUE(!obj->get_script_instance());

			// TODO: Restore serialized state

			CSharpScript::StateBackup &state_backup = script->pending_reload_state[obj_id];

			for (List<Pair<StringName, Variant>>::Element *G = state_backup.properties.front(); G; G = G->next()) {
				obj->get_script_instance()->set(G->get().first, G->get().second);
			}

			// Call OnAfterDeserialization
			CSharpInstance *csi = CAST_CSHARP_INSTANCE(obj->get_script_instance());
			if (csi && csi->script->script_class->implements_interface(CACHED_CLASS(ISerializationListener)))
				obj->get_script_instance()->call_multilevel(string_names.on_after_deserialize);
		}

		script->pending_reload_instances.clear();
	}

#ifdef TOOLS_ENABLED
	// FIXME: Hack to refresh editor in order to display new properties and signals. See if there is a better alternative.
	if (Engine::get_singleton()->is_editor_hint()) {
		EditorNode::get_singleton()->get_inspector()->update_tree();
		NodeDock::singleton->update_lists();
	}
#endif
}
#endif

void CSharpLanguage::_load_scripts_metadata() {
	scripts_metadata.clear();

	String scripts_metadata_filename = "scripts_metadata.";

#ifdef TOOLS_ENABLED
	scripts_metadata_filename += Engine::get_singleton()->is_editor_hint() ? "editor" : "editor_player";
#else
#ifdef DEBUG_ENABLED
	scripts_metadata_filename += "debug";
#else
	scripts_metadata_filename += "release";
#endif
#endif

	String scripts_metadata_path = GodotSharpDirs::get_res_metadata_dir().plus_file(scripts_metadata_filename);

	if (FileAccess::exists(scripts_metadata_path)) {
		String old_json;

		Error ferr = read_all_file_utf8(scripts_metadata_path, old_json);

		ERR_FAIL_COND(ferr != OK);

		Variant old_dict_var;
		String err_str;
		int err_line;
		Error json_err = JSON::parse(old_json, old_dict_var, err_str, err_line);
		if (json_err != OK) {
			ERR_PRINT("Failed to parse metadata file: '" + err_str + "' (" + String::num_int64(err_line) + ").");
			return;
		}

		scripts_metadata = old_dict_var.operator Dictionary();
		scripts_metadata_invalidated = false;

		print_verbose("Successfully loaded scripts metadata");
	} else {
		if (!Engine::get_singleton()->is_editor_hint()) {
			ERR_PRINT("Missing scripts metadata file.");
		}
	}
}

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

void CSharpLanguage::thread_enter() {
#if 0
	if (gdmono && gdmono->is_runtime_initialized()) {
		GDMonoUtils::attach_current_thread();
	}
#endif
}

void CSharpLanguage::thread_exit() {
#if 0
	if (gdmono && gdmono->is_runtime_initialized()) {
		GDMonoUtils::detach_current_thread();
	}
#endif
}

bool CSharpLanguage::debug_break_parse(const String &p_file, int p_line, const String &p_error) {
	// Not a parser error in our case, but it's still used for other type of errors
	if (ScriptDebugger::get_singleton() && Thread::get_caller_id() == Thread::get_main_id()) {
		_debug_parse_err_line = p_line;
		_debug_parse_err_file = p_file;
		_debug_error = p_error;
		ScriptDebugger::get_singleton()->debug(this, false, true);
		return true;
	} else {
		return false;
	}
}

bool CSharpLanguage::debug_break(const String &p_error, bool p_allow_continue) {
	if (ScriptDebugger::get_singleton() && Thread::get_caller_id() == Thread::get_main_id()) {
		_debug_parse_err_line = -1;
		_debug_parse_err_file = "";
		_debug_error = p_error;
		ScriptDebugger::get_singleton()->debug(this, p_allow_continue);
		return true;
	} else {
		return false;
	}
}

void CSharpLanguage::_on_scripts_domain_unloaded() {
	for (Map<Object *, CSharpScriptBinding>::Element *E = script_bindings.front(); E; E = E->next()) {
		CSharpScriptBinding &script_binding = E->value();
		script_binding.inited = false;
	}

	scripts_metadata_invalidated = true;
}

#ifdef TOOLS_ENABLED
void CSharpLanguage::_editor_init_callback() {
	register_editor_internal_calls();

	// Initialize GodotSharpEditor

	GDMonoClass *editor_klass = GDMono::get_singleton()->get_tools_assembly()->get_class("GodotTools", "GodotSharpEditor");
	CRASH_COND(editor_klass == NULL);

	MonoObject *mono_object = mono_object_new(mono_domain_get(), editor_klass->get_mono_ptr());
	CRASH_COND(mono_object == NULL);

	MonoException *exc = NULL;
	GDMonoUtils::runtime_object_init(mono_object, editor_klass, &exc);
	UNHANDLED_EXCEPTION(exc);

	EditorPlugin *godotsharp_editor = Object::cast_to<EditorPlugin>(GDMonoMarshal::mono_object_to_variant(mono_object));
	CRASH_COND(godotsharp_editor == NULL);

	// Enable it as a plugin
	EditorNode::add_editor_plugin(godotsharp_editor);
	ED_SHORTCUT("mono/build_solution", TTR("Build Solution"), KEY_MASK_ALT | KEY_B);
	godotsharp_editor->enable_plugin();

	get_singleton()->godotsharp_editor = godotsharp_editor;
}
#endif

void CSharpLanguage::set_language_index(int p_idx) {
	ERR_FAIL_COND(lang_idx != -1);
	lang_idx = p_idx;
}

void CSharpLanguage::release_script_gchandle(Ref<MonoGCHandle> &p_gchandle) {
	if (!p_gchandle->is_released()) { // Do not lock unnecessarily
		MutexLock lock(get_singleton()->script_gchandle_release_mutex);
		p_gchandle->release();
	}
}

void CSharpLanguage::release_script_gchandle(MonoObject *p_expected_obj, Ref<MonoGCHandle> &p_gchandle) {
	uint32_t pinned_gchandle = MonoGCHandle::new_strong_handle_pinned(p_expected_obj); // We might lock after this, so pin it

	if (!p_gchandle->is_released()) { // Do not lock unnecessarily
		MutexLock lock(get_singleton()->script_gchandle_release_mutex);

		MonoObject *target = p_gchandle->get_target();

		// We release the gchandle if it points to the MonoObject* we expect (otherwise it was
		// already released and could have been replaced) or if we can't get its target MonoObject*
		// (which doesn't necessarily mean it was released, and we want it released in order to
		// avoid locking other threads unnecessarily).
		if (target == p_expected_obj || target == NULL) {
			p_gchandle->release();
		}
	}

	MonoGCHandle::free_handle(pinned_gchandle);
}

CSharpLanguage::CSharpLanguage() {
	ERR_FAIL_COND_MSG(singleton, "C# singleton already exist.");
	singleton = this;

	finalizing = false;

	gdmono = NULL;

	lang_idx = -1;

	scripts_metadata_invalidated = true;

#ifdef TOOLS_ENABLED
	godotsharp_editor = NULL;
#endif
}

CSharpLanguage::~CSharpLanguage() {
	finish();
	singleton = NULL;
}

bool CSharpLanguage::setup_csharp_script_binding(CSharpScriptBinding &r_script_binding, Object *p_object) {
#ifdef DEBUG_ENABLED
	// I don't trust you
	if (p_object->get_script_instance()) {
		CSharpInstance *csharp_instance = CAST_CSHARP_INSTANCE(p_object->get_script_instance());
		CRASH_COND(csharp_instance != NULL && !csharp_instance->is_destructing_script_instance());
	}
#endif

	StringName type_name = p_object->get_class_name();

	// ¯\_(ツ)_/¯
	const ClassDB::ClassInfo *classinfo = ClassDB::classes.getptr(type_name);
	while (classinfo && !classinfo->exposed)
		classinfo = classinfo->inherits_ptr;
	ERR_FAIL_NULL_V(classinfo, false);
	type_name = classinfo->name;

	GDMonoClass *type_class = GDMonoUtils::type_get_proxy_class(type_name);

	ERR_FAIL_NULL_V(type_class, false);

	MonoObject *mono_object = GDMonoUtils::create_managed_for_godot_object(type_class, type_name, p_object);

	ERR_FAIL_NULL_V(mono_object, false);

	r_script_binding.inited = true;
	r_script_binding.type_name = type_name;
	r_script_binding.wrapper_class = type_class; // cache
	r_script_binding.gchandle = MonoGCHandle::create_strong(mono_object);
	r_script_binding.owner = p_object;

	// Tie managed to unmanaged
	Reference *ref = Object::cast_to<Reference>(p_object);

	if (ref) {
		// Unsafe refcount increment. The managed instance also counts as a reference.
		// This way if the unmanaged world has no references to our owner
		// but the managed instance is alive, the refcount will be 1 instead of 0.
		// See: godot_icall_Reference_Dtor(MonoObject *p_obj, Object *p_ptr)

		ref->reference();
		CSharpLanguage::get_singleton()->post_unsafe_reference(ref);
	}

	return true;
}

void *CSharpLanguage::alloc_instance_binding_data(Object *p_object) {
	MutexLock lock(language_bind_mutex);

	Map<Object *, CSharpScriptBinding>::Element *match = script_bindings.find(p_object);
	if (match)
		return (void *)match;

	CSharpScriptBinding script_binding;

	if (!setup_csharp_script_binding(script_binding, p_object))
		return NULL;

	return (void *)insert_script_binding(p_object, script_binding);
}

Map<Object *, CSharpScriptBinding>::Element *CSharpLanguage::insert_script_binding(Object *p_object, const CSharpScriptBinding &p_script_binding) {
	return script_bindings.insert(p_object, p_script_binding);
}

void CSharpLanguage::free_instance_binding_data(Object *p_object, void *p_data) {
	if (GDMono::get_singleton() == NULL) {
#ifdef DEBUG_ENABLED
		CRASH_COND(!script_bindings.empty());
#endif
		// Mono runtime finalized, all the gchandle bindings were already released
		return;
	}

	if (finalizing)
		return; // inside CSharpLanguage::finish(), all the gchandle bindings are released there

	GD_MONO_ASSERT_THREAD_ATTACHED;

	{
		MutexLock lock(language_bind_mutex);

		Map<Object *, CSharpScriptBinding>::Element *data = (Map<Object *, CSharpScriptBinding>::Element *)p_data;

		CSharpScriptBinding &script_binding = data->value();

		if (script_binding.inited) {
			// Set the native instance field to IntPtr.Zero, if not yet garbage collected.
			// This is done to avoid trying to dispose the native instance from Dispose(bool).
			MonoObject *mono_object = script_binding.gchandle->get_target();
			if (mono_object) {
				CACHED_FIELD(GodotObject, ptr)->set_value_raw(mono_object, NULL);
			}
		}

		script_bindings.erase(data);
	}
}

void CSharpLanguage::refcount_incremented_instance_binding(Object *p_object) {
	Reference *ref_owner = Object::cast_to<Reference>(p_object);

#ifdef DEBUG_ENABLED
	CRASH_COND(!ref_owner);
	CRASH_COND(!p_object->has_script_instance_binding(get_language_index()));
#endif

	void *data = p_object->get_script_instance_binding(get_language_index());
	CRASH_COND(!data);

	CSharpScriptBinding &script_binding = ((Map<Object *, CSharpScriptBinding>::Element *)data)->get();
	Ref<MonoGCHandle> &gchandle = script_binding.gchandle;

	if (!script_binding.inited)
		return;

	if (ref_owner->reference_get_count() > 1 && gchandle->is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
		GD_MONO_SCOPE_THREAD_ATTACH;

		// The reference count was increased after the managed side was the only one referencing our owner.
		// This means the owner is being referenced again by the unmanaged side,
		// so the owner must hold the managed side alive again to avoid it from being GCed.

		MonoObject *target = gchandle->get_target();
		if (!target)
			return; // Called after the managed side was collected, so nothing to do here

		// Release the current weak handle and replace it with a strong handle.
		uint32_t strong_gchandle = MonoGCHandle::new_strong_handle(target);
		gchandle->release();
		gchandle->set_handle(strong_gchandle, MonoGCHandle::STRONG_HANDLE);
	}
}

bool CSharpLanguage::refcount_decremented_instance_binding(Object *p_object) {
	Reference *ref_owner = Object::cast_to<Reference>(p_object);

#ifdef DEBUG_ENABLED
	CRASH_COND(!ref_owner);
	CRASH_COND(!p_object->has_script_instance_binding(get_language_index()));
#endif

	void *data = p_object->get_script_instance_binding(get_language_index());
	CRASH_COND(!data);

	CSharpScriptBinding &script_binding = ((Map<Object *, CSharpScriptBinding>::Element *)data)->get();
	Ref<MonoGCHandle> &gchandle = script_binding.gchandle;

	int refcount = ref_owner->reference_get_count();

	if (!script_binding.inited)
		return refcount == 0;

	if (refcount == 1 && gchandle.is_valid() && !gchandle->is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
		GD_MONO_SCOPE_THREAD_ATTACH;

		// If owner owner is no longer referenced by the unmanaged side,
		// the managed instance takes responsibility of deleting the owner when GCed.

		MonoObject *target = gchandle->get_target();
		if (!target)
			return refcount == 0; // Called after the managed side was collected, so nothing to do here

		// Release the current strong handle and replace it with a weak handle.
		uint32_t weak_gchandle = MonoGCHandle::new_weak_handle(target);
		gchandle->release();
		gchandle->set_handle(weak_gchandle, MonoGCHandle::WEAK_HANDLE);

		return false;
	}

	return refcount == 0;
}

CSharpInstance *CSharpInstance::create_for_managed_type(Object *p_owner, CSharpScript *p_script, const Ref<MonoGCHandle> &p_gchandle) {
	CSharpInstance *instance = memnew(CSharpInstance);

	Reference *ref = Object::cast_to<Reference>(p_owner);

	instance->base_ref = ref != NULL;
	instance->script = Ref<CSharpScript>(p_script);
	instance->owner = p_owner;
	instance->gchandle = p_gchandle;

	if (instance->base_ref)
		instance->_reference_owner_unsafe();

	p_script->instances.insert(p_owner);

	return instance;
}

MonoObject *CSharpInstance::get_mono_object() const {
	ERR_FAIL_COND_V(gchandle.is_null(), NULL);
	return gchandle->get_target();
}

Object *CSharpInstance::get_owner() {
	return owner;
}

bool CSharpInstance::set(const StringName &p_name, const Variant &p_value) {
	ERR_FAIL_COND_V(!script.is_valid(), false);

	GD_MONO_SCOPE_THREAD_ATTACH;

	MonoObject *mono_object = get_mono_object();
	ERR_FAIL_NULL_V(mono_object, false);

	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		GDMonoField *field = top->get_field(p_name);

		if (field) {
			field->set_value_from_variant(mono_object, p_value);
			return true;
		}

		GDMonoProperty *property = top->get_property(p_name);

		if (property) {
			property->set_value_from_variant(mono_object, p_value);
			return true;
		}

		top = top->get_parent_class();
	}

	// Call _set

	top = script->script_class;

	while (top && top != script->native) {
		GDMonoMethod *method = top->get_method(CACHED_STRING_NAME(_set), 2);

		if (method) {
			Variant name = p_name;
			const Variant *args[2] = { &name, &p_value };

			MonoObject *ret = method->invoke(mono_object, args);

			if (ret && GDMonoMarshal::unbox<MonoBoolean>(ret))
				return true;

			break;
		}

		top = top->get_parent_class();
	}

	return false;
}

bool CSharpInstance::get(const StringName &p_name, Variant &r_ret) const {
	ERR_FAIL_COND_V(!script.is_valid(), false);

	GD_MONO_SCOPE_THREAD_ATTACH;

	MonoObject *mono_object = get_mono_object();
	ERR_FAIL_NULL_V(mono_object, false);

	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		GDMonoField *field = top->get_field(p_name);

		if (field) {
			MonoObject *value = field->get_value(mono_object);
			r_ret = GDMonoMarshal::mono_object_to_variant(value);
			return true;
		}

		GDMonoProperty *property = top->get_property(p_name);

		if (property) {
			MonoException *exc = NULL;
			MonoObject *value = property->get_value(mono_object, &exc);
			if (exc) {
				r_ret = Variant();
				GDMonoUtils::set_pending_exception(exc);
			} else {
				r_ret = GDMonoMarshal::mono_object_to_variant(value);
			}
			return true;
		}

		top = top->get_parent_class();
	}

	// Call _get

	top = script->script_class;

	while (top && top != script->native) {
		GDMonoMethod *method = top->get_method(CACHED_STRING_NAME(_get), 1);

		if (method) {
			Variant name = p_name;
			const Variant *args[1] = { &name };

			MonoObject *ret = method->invoke(mono_object, args);

			if (ret) {
				r_ret = GDMonoMarshal::mono_object_to_variant(ret);
				return true;
			}

			break;
		}

		top = top->get_parent_class();
	}

	return false;
}

void CSharpInstance::get_properties_state_for_reloading(List<Pair<StringName, Variant>> &r_state) {
	List<PropertyInfo> pinfo;
	get_property_list(&pinfo);

	for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
		Pair<StringName, Variant> state_pair;
		state_pair.first = E->get().name;

		ManagedType managedType;

		GDMonoField *field = nullptr;
		GDMonoClass *top = script->script_class;
		while (top && top != script->native) {
			field = top->get_field(state_pair.first);
			if (field) {
				break;
			}

			top = top->get_parent_class();
		}
		if (!field) {
			continue; // Properties ignored. We get the property baking fields instead.
		}

		managedType = field->get_type();

		if (GDMonoMarshal::managed_to_variant_type(managedType) != Variant::NIL) { // If we can marshal it
			if (get(state_pair.first, state_pair.second)) {
				r_state.push_back(state_pair);
			}
		}
	}
}

void CSharpInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	List<PropertyInfo> props;
	for (OrderedHashMap<StringName, PropertyInfo>::ConstElement E = script->member_info.front(); E; E = E.next()) {
		props.push_front(E.value());
	}

	// Call _get_property_list

	ERR_FAIL_COND(!script.is_valid());

	GD_MONO_SCOPE_THREAD_ATTACH;

	MonoObject *mono_object = get_mono_object();
	ERR_FAIL_NULL(mono_object);

	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		GDMonoMethod *method = top->get_method(CACHED_STRING_NAME(_get_property_list), 0);

		if (method) {
			MonoObject *ret = method->invoke(mono_object);

			if (ret) {
				Array array = Array(GDMonoMarshal::mono_object_to_variant(ret));
				for (int i = 0, size = array.size(); i < size; i++) {
					props.push_back(PropertyInfo::from_dict(array.get(i)));
				}
			}

			break;
		}

		top = top->get_parent_class();
	}

	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		p_properties->push_back(E->get());
	}
}

Variant::Type CSharpInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	if (script->member_info.has(p_name)) {
		if (r_is_valid)
			*r_is_valid = true;
		return script->member_info[p_name].type;
	}

	if (r_is_valid)
		*r_is_valid = false;

	return Variant::NIL;
}

void CSharpInstance::get_method_list(List<MethodInfo> *p_list) const {
	if (!script->is_valid() || !script->script_class) {
		return;
	}

	GD_MONO_SCOPE_THREAD_ATTACH;

	// TODO: We're filtering out constructors but there may be other methods unsuitable for explicit calls.
	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		const Vector<GDMonoMethod *> &methods = top->get_all_methods();
		for (int i = 0; i < methods.size(); ++i) {
			MethodInfo minfo = methods[i]->get_method_info();
			if (minfo.name != CACHED_STRING_NAME(dotctor)) {
				p_list->push_back(minfo);
			}
		}

		top = top->get_parent_class();
	}
}

bool CSharpInstance::has_method(const StringName &p_method) const {
	if (!script.is_valid())
		return false;

	GD_MONO_SCOPE_THREAD_ATTACH;

	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		if (top->has_fetched_method_unknown_params(p_method)) {
			return true;
		}

		top = top->get_parent_class();
	}

	return false;
}

Variant CSharpInstance::call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	ERR_FAIL_COND_V(!script.is_valid(), Variant());

	GD_MONO_SCOPE_THREAD_ATTACH;

	MonoObject *mono_object = get_mono_object();

	if (!mono_object) {
		r_error.error = Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		ERR_FAIL_V(Variant());
	}

	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		GDMonoMethod *method = top->get_method(p_method, p_argcount);

		if (method) {
			MonoObject *return_value = method->invoke(mono_object, p_args);

			r_error.error = Variant::CallError::CALL_OK;

			if (return_value) {
				return GDMonoMarshal::mono_object_to_variant(return_value);
			} else {
				return Variant();
			}
		}

		top = top->get_parent_class();
	}

	r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;

	return Variant();
}

void CSharpInstance::call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount) {
	GD_MONO_SCOPE_THREAD_ATTACH;

	if (script.is_valid()) {
		MonoObject *mono_object = get_mono_object();

		ERR_FAIL_NULL(mono_object);

		_call_multilevel(mono_object, p_method, p_args, p_argcount);
	}
}

void CSharpInstance::_call_multilevel(MonoObject *p_mono_object, const StringName &p_method, const Variant **p_args, int p_argcount) {
	GD_MONO_ASSERT_THREAD_ATTACHED;

	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		GDMonoMethod *method = top->get_method(p_method, p_argcount);

		if (method) {
			method->invoke(p_mono_object, p_args);
			return;
		}

		top = top->get_parent_class();
	}
}

void CSharpInstance::call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount) {
	// Sorry, the method is the one that controls the call order

	call_multilevel(p_method, p_args, p_argcount);
}

bool CSharpInstance::_reference_owner_unsafe() {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref);
	CRASH_COND(owner == NULL);
	CRASH_COND(unsafe_referenced); // already referenced
#endif

	// Unsafe refcount increment. The managed instance also counts as a reference.
	// This way if the unmanaged world has no references to our owner
	// but the managed instance is alive, the refcount will be 1 instead of 0.
	// See: _unreference_owner_unsafe()

	// May not me referenced yet, so we must use init_ref() instead of reference()
	if (static_cast<Reference *>(owner)->init_ref()) {
		CSharpLanguage::get_singleton()->post_unsafe_reference(owner);
		unsafe_referenced = true;
	}

	return unsafe_referenced;
}

bool CSharpInstance::_unreference_owner_unsafe() {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref);
	CRASH_COND(owner == NULL);
#endif

	if (!unsafe_referenced)
		return false; // Already unreferenced

	unsafe_referenced = false;

	// Called from CSharpInstance::mono_object_disposed() or ~CSharpInstance()

	// Unsafe refcount decrement. The managed instance also counts as a reference.
	// See: _reference_owner_unsafe()

	// Destroying the owner here means self destructing, so we defer the owner destruction to the caller.
	CSharpLanguage::get_singleton()->pre_unsafe_unreference(owner);
	return static_cast<Reference *>(owner)->unreference();
}

MonoObject *CSharpInstance::_internal_new_managed() {
#ifdef DEBUG_ENABLED
	CRASH_COND(!gchandle.is_valid());
#endif

	// Search the constructor first, to fail with an error if it's not found before allocating anything else.
	GDMonoMethod *ctor = script->script_class->get_method(CACHED_STRING_NAME(dotctor), 0);
	ERR_FAIL_NULL_V_MSG(ctor, NULL,
			"Cannot create script instance because the class does not define a parameterless constructor: '" + script->get_path() + "'.");

	CSharpLanguage::get_singleton()->release_script_gchandle(gchandle);

	ERR_FAIL_NULL_V(owner, NULL);
	ERR_FAIL_COND_V(script.is_null(), NULL);

	MonoObject *mono_object = mono_object_new(mono_domain_get(), script->script_class->get_mono_ptr());

	if (!mono_object) {
		// Important to clear this before destroying the script instance here
		script = Ref<CSharpScript>();

		bool die = _unreference_owner_unsafe();
		// Not ok for the owner to die here. If there is a situation where this can happen, it will be considered a bug.
		CRASH_COND(die);

		owner = NULL;

		ERR_FAIL_V_MSG(NULL, "Failed to allocate memory for the object.");
	}

	// Tie managed to unmanaged
	gchandle = MonoGCHandle::create_strong(mono_object);

	if (base_ref)
		_reference_owner_unsafe(); // Here, after assigning the gchandle (for the refcount_incremented callback)

	CACHED_FIELD(GodotObject, ptr)->set_value_raw(mono_object, owner);

	// Construct
	ctor->invoke_raw(mono_object, NULL);

	return mono_object;
}

void CSharpInstance::mono_object_disposed(MonoObject *p_obj) {
#ifdef DEBUG_ENABLED
	CRASH_COND(base_ref);
	CRASH_COND(gchandle.is_null());
#endif
	CSharpLanguage::get_singleton()->release_script_gchandle(p_obj, gchandle);
}

void CSharpInstance::mono_object_disposed_baseref(MonoObject *p_obj, bool p_is_finalizer, bool &r_delete_owner, bool &r_remove_script_instance) {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref);
	CRASH_COND(gchandle.is_null());
#endif

	r_remove_script_instance = false;

	if (_unreference_owner_unsafe()) {
		// Safe to self destruct here with memdelete(owner), but it's deferred to the caller to prevent future mistakes.
		r_delete_owner = true;
	} else {
		r_delete_owner = false;
		CSharpLanguage::get_singleton()->release_script_gchandle(p_obj, gchandle);

		if (!p_is_finalizer) {
			// If the native instance is still alive and Dispose() was called
			// (instead of the finalizer), then we remove the script instance.
			r_remove_script_instance = true;
		} else if (!GDMono::get_singleton()->is_finalizing_scripts_domain()) {
			// If the native instance is still alive and this is called from the finalizer,
			// then it was referenced from another thread before the finalizer could
			// unreference and delete it, so we want to keep it.
			// GC.ReRegisterForFinalize(this) is not safe because the objects referenced by 'this'
			// could have already been collected. Instead we will create a new managed instance here.
			MonoObject *new_managed = _internal_new_managed();
			if (!new_managed) {
				r_remove_script_instance = true;
			}
		}
	}
}

void CSharpInstance::refcount_incremented() {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref);
	CRASH_COND(owner == NULL);
#endif

	Reference *ref_owner = Object::cast_to<Reference>(owner);

	if (ref_owner->reference_get_count() > 1 && gchandle->is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
		GD_MONO_SCOPE_THREAD_ATTACH;

		// The reference count was increased after the managed side was the only one referencing our owner.
		// This means the owner is being referenced again by the unmanaged side,
		// so the owner must hold the managed side alive again to avoid it from being GCed.

		// Release the current weak handle and replace it with a strong handle.
		uint32_t strong_gchandle = MonoGCHandle::new_strong_handle(gchandle->get_target());
		gchandle->release();
		gchandle->set_handle(strong_gchandle, MonoGCHandle::STRONG_HANDLE);
	}
}

bool CSharpInstance::refcount_decremented() {
#ifdef DEBUG_ENABLED
	CRASH_COND(!base_ref);
	CRASH_COND(owner == NULL);
#endif

	Reference *ref_owner = Object::cast_to<Reference>(owner);

	int refcount = ref_owner->reference_get_count();

	if (refcount == 1 && !gchandle->is_weak()) { // The managed side also holds a reference, hence 1 instead of 0
		GD_MONO_SCOPE_THREAD_ATTACH;

		// If owner owner is no longer referenced by the unmanaged side,
		// the managed instance takes responsibility of deleting the owner when GCed.

		// Release the current strong handle and replace it with a weak handle.
		uint32_t weak_gchandle = MonoGCHandle::new_weak_handle(gchandle->get_target());
		gchandle->release();
		gchandle->set_handle(weak_gchandle, MonoGCHandle::WEAK_HANDLE);

		return false;
	}

	ref_dying = (refcount == 0);

	return ref_dying;
}

MultiplayerAPI::RPCMode CSharpInstance::_member_get_rpc_mode(IMonoClassMember *p_member) const {
	if (p_member->has_attribute(CACHED_CLASS(RemoteAttribute)))
		return MultiplayerAPI::RPC_MODE_REMOTE;
	if (p_member->has_attribute(CACHED_CLASS(MasterAttribute)))
		return MultiplayerAPI::RPC_MODE_MASTER;
	if (p_member->has_attribute(CACHED_CLASS(PuppetAttribute)))
		return MultiplayerAPI::RPC_MODE_PUPPET;
	if (p_member->has_attribute(CACHED_CLASS(SlaveAttribute)))
		return MultiplayerAPI::RPC_MODE_PUPPET;
	if (p_member->has_attribute(CACHED_CLASS(RemoteSyncAttribute)))
		return MultiplayerAPI::RPC_MODE_REMOTESYNC;
	if (p_member->has_attribute(CACHED_CLASS(SyncAttribute)))
		return MultiplayerAPI::RPC_MODE_REMOTESYNC;
	if (p_member->has_attribute(CACHED_CLASS(MasterSyncAttribute)))
		return MultiplayerAPI::RPC_MODE_MASTERSYNC;
	if (p_member->has_attribute(CACHED_CLASS(PuppetSyncAttribute)))
		return MultiplayerAPI::RPC_MODE_PUPPETSYNC;

	return MultiplayerAPI::RPC_MODE_DISABLED;
}

MultiplayerAPI::RPCMode CSharpInstance::get_rpc_mode(const StringName &p_method) const {
	GD_MONO_SCOPE_THREAD_ATTACH;

	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		GDMonoMethod *method = top->get_fetched_method_unknown_params(p_method);

		if (method && !method->is_static())
			return _member_get_rpc_mode(method);

		top = top->get_parent_class();
	}

	return MultiplayerAPI::RPC_MODE_DISABLED;
}

MultiplayerAPI::RPCMode CSharpInstance::get_rset_mode(const StringName &p_variable) const {
	GD_MONO_SCOPE_THREAD_ATTACH;

	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		GDMonoField *field = top->get_field(p_variable);

		if (field && !field->is_static())
			return _member_get_rpc_mode(field);

		GDMonoProperty *property = top->get_property(p_variable);

		if (property && !property->is_static())
			return _member_get_rpc_mode(property);

		top = top->get_parent_class();
	}

	return MultiplayerAPI::RPC_MODE_DISABLED;
}

void CSharpInstance::notification(int p_notification) {
	GD_MONO_SCOPE_THREAD_ATTACH;

	if (p_notification == Object::NOTIFICATION_PREDELETE) {
		// When NOTIFICATION_PREDELETE is sent, we also take the chance to call Dispose().
		// It's safe to call Dispose() multiple times and NOTIFICATION_PREDELETE is guaranteed
		// to be sent at least once, which happens right before the call to the destructor.

		predelete_notified = true;

		if (base_ref) {
			// It's not safe to proceed if the owner derives Reference and the refcount reached 0.
			// At this point, Dispose() was already called (manually or from the finalizer) so
			// that's not a problem. The refcount wouldn't have reached 0 otherwise, since the
			// managed side references it and Dispose() needs to be called to release it.
			// However, this means C# Reference scripts can't receive NOTIFICATION_PREDELETE, but
			// this is likely the case with GDScript as well: https://github.com/godotengine/godot/issues/6784
			return;
		}

		_call_notification(p_notification);

		MonoObject *mono_object = get_mono_object();
		ERR_FAIL_NULL(mono_object);

		MonoException *exc = NULL;
		GDMonoUtils::dispose(mono_object, &exc);

		if (exc) {
			GDMonoUtils::set_pending_exception(exc);
		}

		return;
	}

	_call_notification(p_notification);
}

void CSharpInstance::_call_notification(int p_notification) {
	GD_MONO_ASSERT_THREAD_ATTACHED;

	MonoObject *mono_object = get_mono_object();
	ERR_FAIL_NULL(mono_object);

	// Custom version of _call_multilevel, optimized for _notification

	uint32_t arg = p_notification;
	void *args[1] = { &arg };
	StringName method_name = CACHED_STRING_NAME(_notification);

	GDMonoClass *top = script->script_class;

	while (top && top != script->native) {
		GDMonoMethod *method = top->get_method(method_name, 1);

		if (method) {
			method->invoke_raw(mono_object, args);
			return;
		}

		top = top->get_parent_class();
	}
}

String CSharpInstance::to_string(bool *r_valid) {
	GD_MONO_SCOPE_THREAD_ATTACH;

	MonoObject *mono_object = get_mono_object();

	if (mono_object == NULL) {
		if (r_valid)
			*r_valid = false;
		return String();
	}

	MonoException *exc = NULL;
	MonoString *result = GDMonoUtils::object_to_string(mono_object, &exc);

	if (exc) {
		GDMonoUtils::set_pending_exception(exc);
		if (r_valid)
			*r_valid = false;
		return String();
	}

	if (result == NULL) {
		if (r_valid)
			*r_valid = false;
		return String();
	}

	return GDMonoMarshal::mono_string_to_godot(result);
}

Ref<Script> CSharpInstance::get_script() const {
	return script;
}

ScriptLanguage *CSharpInstance::get_language() {
	return CSharpLanguage::get_singleton();
}

CSharpInstance::CSharpInstance() :
		owner(NULL),
		base_ref(false),
		ref_dying(false),
		unsafe_referenced(false),
		predelete_notified(false),
		destructing_script_instance(false) {
}

CSharpInstance::~CSharpInstance() {
	GD_MONO_SCOPE_THREAD_ATTACH;

	destructing_script_instance = true;

	if (gchandle.is_valid()) {
		if (!predelete_notified && !ref_dying) {
			// This destructor is not called from the owners destructor.
			// This could be being called from the owner's set_script_instance method,
			// meaning this script is being replaced with another one. If this is the case,
			// we must call Dispose here, because Dispose calls owner->set_script_instance(NULL)
			// and that would mess up with the new script instance if called later.

			MonoObject *mono_object = gchandle->get_target();

			if (mono_object) {
				MonoException *exc = NULL;
				GDMonoUtils::dispose(mono_object, &exc);

				if (exc) {
					GDMonoUtils::set_pending_exception(exc);
				}
			}
		}

		gchandle->release(); // Make sure the gchandle is released
	}

	// If not being called from the owner's destructor, and we still hold a reference to the owner
	if (base_ref && !ref_dying && owner && unsafe_referenced) {
		// The owner's script or script instance is being replaced (or removed)

		// Transfer ownership to an "instance binding"

		Reference *ref_owner = static_cast<Reference *>(owner);

		// We will unreference the owner before referencing it again, so we need to keep it alive
		Ref<Reference> scope_keep_owner_alive(ref_owner);
		(void)scope_keep_owner_alive;

		// Unreference the owner here, before the new "instance binding" references it.
		// Otherwise, the unsafe reference debug checks will incorrectly detect a bug.
		bool die = _unreference_owner_unsafe();
		CRASH_COND(die); // `owner_keep_alive` holds a reference, so it can't die

		void *data = owner->get_script_instance_binding(CSharpLanguage::get_singleton()->get_language_index());
		CRASH_COND(data == NULL);

		CSharpScriptBinding &script_binding = ((Map<Object *, CSharpScriptBinding>::Element *)data)->get();

		if (!script_binding.inited) {
			MutexLock lock(CSharpLanguage::get_singleton()->get_language_bind_mutex());

			if (!script_binding.inited) { // Other thread may have set it up
				// Already had a binding that needs to be setup
				CSharpLanguage::get_singleton()->setup_csharp_script_binding(script_binding, owner);
				CRASH_COND(!script_binding.inited);
			}
		}

#ifdef DEBUG_ENABLED
		// The "instance binding" holds a reference so the refcount should be at least 2 before `scope_keep_owner_alive` goes out of scope
		CRASH_COND(ref_owner->reference_get_count() <= 1);
#endif
	}

	if (script.is_valid() && owner) {
		MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);

#ifdef DEBUG_ENABLED
		// CSharpInstance must not be created unless it's going to be added to the list for sure
		Set<Object *>::Element *match = script->instances.find(owner);
		CRASH_COND(!match);
		script->instances.erase(match);
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
void CSharpScript::_update_exports_values(Map<StringName, Variant> &values, List<PropertyInfo> &propnames) {
	if (base_cache.is_valid()) {
		base_cache->_update_exports_values(values, propnames);
	}

	for (Map<StringName, Variant>::Element *E = exported_members_defval_cache.front(); E; E = E->next()) {
		values[E->key()] = E->get();
	}

	for (List<PropertyInfo>::Element *E = exported_members_cache.front(); E; E = E->next()) {
		propnames.push_back(E->get());
	}
}

void CSharpScript::_update_member_info_no_exports() {
	if (exports_invalidated) {
		GD_MONO_ASSERT_THREAD_ATTACHED;

		exports_invalidated = false;

		member_info.clear();

		GDMonoClass *top = script_class;

		while (top && top != native) {
			PropertyInfo prop_info;
			bool exported;

			const Vector<GDMonoField *> &fields = top->get_all_fields();

			for (int i = fields.size() - 1; i >= 0; i--) {
				GDMonoField *field = fields[i];

				if (_get_member_export(field, /* inspect export: */ false, prop_info, exported)) {
					StringName member_name = field->get_name();

					member_info[member_name] = prop_info;
					exported_members_cache.push_front(prop_info);
					exported_members_defval_cache[member_name] = Variant();
				}
			}

			const Vector<GDMonoProperty *> &properties = top->get_all_properties();

			for (int i = properties.size() - 1; i >= 0; i--) {
				GDMonoProperty *property = properties[i];

				if (_get_member_export(property, /* inspect export: */ false, prop_info, exported)) {
					StringName member_name = property->get_name();

					member_info[member_name] = prop_info;
					exported_members_cache.push_front(prop_info);
					exported_members_defval_cache[member_name] = Variant();
				}
			}

			top = top->get_parent_class();
		}
	}
}
#endif

bool CSharpScript::_update_exports(PlaceHolderScriptInstance *p_instance_to_update) {
#ifdef TOOLS_ENABLED
	bool is_editor = Engine::get_singleton()->is_editor_hint();
	if (is_editor)
		placeholder_fallback_enabled = true; // until proven otherwise
#endif

	if (!valid)
		return false;

	bool changed = false;

#ifdef TOOLS_ENABLED
	if (!is_editor || exports_invalidated)
#endif
	{
		GD_MONO_SCOPE_THREAD_ATTACH;

		changed = true;

		member_info.clear();

#ifdef TOOLS_ENABLED
		MonoObject *tmp_object = nullptr;
		Object *tmp_native = nullptr;
		uint32_t tmp_pinned_gchandle = 0;

		if (is_editor) {
			exports_invalidated = false;

			exported_members_cache.clear();
			exported_members_defval_cache.clear();

			// Here we create a temporary managed instance of the class to get the initial values
			tmp_object = mono_object_new(mono_domain_get(), script_class->get_mono_ptr());

			if (!tmp_object) {
				ERR_PRINT("Failed to allocate temporary MonoObject.");
				return false;
			}

			tmp_pinned_gchandle = MonoGCHandle::new_strong_handle_pinned(tmp_object); // pin it (not sure if needed)

			GDMonoMethod *ctor = script_class->get_method(CACHED_STRING_NAME(dotctor), 0);

			ERR_FAIL_NULL_V_MSG(ctor, NULL,
					"Cannot construct temporary MonoObject because the class does not define a parameterless constructor: '" + get_path() + "'.");

			MonoException *ctor_exc = NULL;
			ctor->invoke(tmp_object, NULL, &ctor_exc);

			tmp_native = GDMonoMarshal::unbox<Object *>(CACHED_FIELD(GodotObject, ptr)->get_value(tmp_object));

			if (ctor_exc) {
				// TODO: Should we free 'tmp_native' if the exception was thrown after its creation?

				MonoGCHandle::free_handle(tmp_pinned_gchandle);
				tmp_object = NULL;

				ERR_PRINT("Exception thrown from constructor of temporary MonoObject:");
				GDMonoUtils::debug_print_unhandled_exception(ctor_exc);
				return false;
			}
		}
#endif

		GDMonoClass *top = script_class;

		while (top && top != native) {
			PropertyInfo prop_info;
			bool exported;

			const Vector<GDMonoField *> &fields = top->get_all_fields();

			for (int i = fields.size() - 1; i >= 0; i--) {
				GDMonoField *field = fields[i];

				if (_get_member_export(field, /* inspect export: */ true, prop_info, exported)) {
					StringName member_name = field->get_name();

					member_info[member_name] = prop_info;

					if (exported) {
#ifdef TOOLS_ENABLED
						if (is_editor) {
							exported_members_cache.push_front(prop_info);

							if (tmp_object) {
								exported_members_defval_cache[member_name] = GDMonoMarshal::mono_object_to_variant(field->get_value(tmp_object));
							}
						}
#endif

#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
						exported_members_names.insert(member_name);
#endif
					}
				}
			}

			const Vector<GDMonoProperty *> &properties = top->get_all_properties();

			for (int i = properties.size() - 1; i >= 0; i--) {
				GDMonoProperty *property = properties[i];

				if (_get_member_export(property, /* inspect export: */ true, prop_info, exported)) {
					StringName member_name = property->get_name();

					member_info[member_name] = prop_info;

					if (exported) {
#ifdef TOOLS_ENABLED
						if (is_editor) {
							exported_members_cache.push_front(prop_info);
							if (tmp_object) {
								MonoException *exc = nullptr;
								MonoObject *ret = property->get_value(tmp_object, &exc);
								if (exc) {
									exported_members_defval_cache[member_name] = Variant();
									GDMonoUtils::debug_print_unhandled_exception(exc);
								} else {
									exported_members_defval_cache[member_name] = GDMonoMarshal::mono_object_to_variant(ret);
								}
							}
						}
#endif

#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
						exported_members_names.insert(member_name);
#endif
					}
				}
			}

			top = top->get_parent_class();
		}

#ifdef TOOLS_ENABLED
		if (is_editor) {
			// Need to check this here, before disposal
			bool base_ref = Object::cast_to<Reference>(tmp_native) != NULL;

			// Dispose the temporary managed instance

			MonoException *exc = NULL;
			GDMonoUtils::dispose(tmp_object, &exc);

			if (exc) {
				ERR_PRINT("Exception thrown from method Dispose() of temporary MonoObject:");
				GDMonoUtils::debug_print_unhandled_exception(exc);
			}

			MonoGCHandle::free_handle(tmp_pinned_gchandle);
			tmp_object = NULL;

			if (tmp_native && !base_ref) {
				Node *node = Object::cast_to<Node>(tmp_native);
				if (node && node->is_inside_tree()) {
					ERR_PRINT("Temporary instance was added to the scene tree.");
				} else {
					memdelete(tmp_native);
				}
			}
		}
#endif
	}

#ifdef TOOLS_ENABLED
	if (is_editor) {
		placeholder_fallback_enabled = false;

		if ((changed || p_instance_to_update) && placeholders.size()) {
			// Update placeholders if any
			Map<StringName, Variant> values;
			List<PropertyInfo> propnames;
			_update_exports_values(values, propnames);

			if (changed) {
				for (Set<PlaceHolderScriptInstance *>::Element *E = placeholders.front(); E; E = E->next()) {
					E->get()->update(propnames, values);
				}
			} else {
				p_instance_to_update->update(propnames, values);
			}
		}
	}
#endif

	return changed;
}

void CSharpScript::load_script_signals(GDMonoClass *p_class, GDMonoClass *p_native_class) {
	// no need to load the script's signals more than once
	if (!signals_invalidated) {
		return;
	}

	// make sure this classes signals are empty when loading for the first time
	_signals.clear();

	GD_MONO_SCOPE_THREAD_ATTACH;

	GDMonoClass *top = p_class;
	while (top && top != p_native_class) {
		const Vector<GDMonoClass *> &delegates = top->get_all_delegates();
		for (int i = delegates.size() - 1; i >= 0; --i) {
			Vector<Argument> parameters;

			GDMonoClass *delegate = delegates[i];

			if (_get_signal(top, delegate, parameters)) {
				_signals[delegate->get_name()] = parameters;
			}
		}

		top = top->get_parent_class();
	}

	signals_invalidated = false;
}

bool CSharpScript::_get_signal(GDMonoClass *p_class, GDMonoClass *p_delegate, Vector<Argument> &params) {
	GD_MONO_ASSERT_THREAD_ATTACHED;

	if (p_delegate->has_attribute(CACHED_CLASS(SignalAttribute))) {
		MonoType *raw_type = p_delegate->get_mono_type();

		if (mono_type_get_type(raw_type) == MONO_TYPE_CLASS) {
			// Arguments are accessibles as arguments of .Invoke method
			GDMonoMethod *invoke = p_delegate->get_method(mono_get_delegate_invoke(p_delegate->get_mono_ptr()));

			Vector<StringName> names;
			Vector<ManagedType> types;
			invoke->get_parameter_names(names);
			invoke->get_parameter_types(types);

			if (names.size() == types.size()) {
				for (int i = 0; i < names.size(); ++i) {
					Argument arg;
					arg.name = names[i];
					arg.type = GDMonoMarshal::managed_to_variant_type(types[i]);

					if (arg.type == Variant::NIL) {
						ERR_PRINT("Unknown type of signal parameter: '" + arg.name + "' in '" + p_class->get_full_name() + "'.");
						return false;
					}

					params.push_back(arg);
				}

				return true;
			}
		}
	}

	return false;
}

/**
 * Returns false if there was an error, otherwise true.
 * If there was an error, r_prop_info and r_exported are not assigned any value.
 */
bool CSharpScript::_get_member_export(IMonoClassMember *p_member, bool p_inspect_export, PropertyInfo &r_prop_info, bool &r_exported) {
	GD_MONO_ASSERT_THREAD_ATTACHED;

	// Goddammit, C++. All I wanted was some nested functions.
#define MEMBER_FULL_QUALIFIED_NAME(m_member) \
	(m_member->get_enclosing_class()->get_full_name() + "." + (String)m_member->get_name())

	if (p_member->is_static()) {
#ifdef TOOLS_ENABLED
		if (p_member->has_attribute(CACHED_CLASS(ExportAttribute)))
			ERR_PRINT("Cannot export member because it is static: '" + MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");
#endif
		return false;
	}

	if (member_info.has(p_member->get_name()))
		return false;

	ManagedType type;

	if (p_member->get_member_type() == IMonoClassMember::MEMBER_TYPE_FIELD) {
		type = static_cast<GDMonoField *>(p_member)->get_type();
	} else if (p_member->get_member_type() == IMonoClassMember::MEMBER_TYPE_PROPERTY) {
		type = static_cast<GDMonoProperty *>(p_member)->get_type();
	} else {
		CRASH_NOW();
	}

	bool exported = p_member->has_attribute(CACHED_CLASS(ExportAttribute));

	if (p_member->get_member_type() == IMonoClassMember::MEMBER_TYPE_PROPERTY) {
		GDMonoProperty *property = static_cast<GDMonoProperty *>(p_member);
		if (!property->has_getter()) {
#ifdef TOOLS_ENABLED
			if (exported)
				ERR_PRINT("Cannot export a property without a getter: '" + MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");
#endif
			return false;
		}
		if (!property->has_setter()) {
#ifdef TOOLS_ENABLED
			if (exported)
				ERR_PRINT("Cannot export a property without a setter: '" + MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");
#endif
			return false;
		}
	}

	Variant::Type variant_type = GDMonoMarshal::managed_to_variant_type(type);

	if (!p_inspect_export || !exported) {
		r_prop_info = PropertyInfo(variant_type, (String)p_member->get_name(), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_SCRIPT_VARIABLE);
		r_exported = false;
		return true;
	}

#ifdef TOOLS_ENABLED
	MonoObject *attr = p_member->get_attribute(CACHED_CLASS(ExportAttribute));
#endif

	PropertyHint hint = PROPERTY_HINT_NONE;
	String hint_string;

	if (variant_type == Variant::NIL) {
#ifdef TOOLS_ENABLED
		ERR_PRINT("Unknown exported member type: '" + MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");
#endif
		return false;
	}

#ifdef TOOLS_ENABLED
	int hint_res = _try_get_member_export_hint(p_member, type, variant_type, /* allow_generics: */ true, hint, hint_string);

	ERR_FAIL_COND_V_MSG(hint_res == -1, false,
			"Error while trying to determine information about the exported member: '" +
					MEMBER_FULL_QUALIFIED_NAME(p_member) + "'.");

	if (hint_res == 0) {
		hint = PropertyHint(CACHED_FIELD(ExportAttribute, hint)->get_int_value(attr));
		hint_string = CACHED_FIELD(ExportAttribute, hintString)->get_string_value(attr);
	}
#endif

	r_prop_info = PropertyInfo(variant_type, (String)p_member->get_name(), hint, hint_string, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE);
	r_exported = true;

	return true;

#undef MEMBER_FULL_QUALIFIED_NAME
}

#ifdef TOOLS_ENABLED
int CSharpScript::_try_get_member_export_hint(IMonoClassMember *p_member, ManagedType p_type, Variant::Type p_variant_type, bool p_allow_generics, PropertyHint &r_hint, String &r_hint_string) {
	GD_MONO_ASSERT_THREAD_ATTACHED;

	if (p_variant_type == Variant::INT && p_type.type_encoding == MONO_TYPE_VALUETYPE && mono_class_is_enum(p_type.type_class->get_mono_ptr())) {
		MonoReflectionType *reftype = mono_type_get_object(mono_domain_get(), p_type.type_class->get_mono_type());
		r_hint = GDMonoUtils::Marshal::type_has_flags_attribute(reftype) ? PROPERTY_HINT_FLAGS : PROPERTY_HINT_ENUM;

		Vector<MonoClassField *> fields = p_type.type_class->get_enum_fields();

		MonoType *enum_basetype = mono_class_enum_basetype(p_type.type_class->get_mono_ptr());

		String name_only_hint_string;

		// True: enum Foo { Bar, Baz, Quux }
		// True: enum Foo { Bar = 0, Baz = 1, Quux = 2 }
		// False: enum Foo { Bar = 0, Baz = 7, Quux = 5 }
		bool uses_default_values = true;

		for (int i = 0; i < fields.size(); i++) {
			MonoClassField *field = fields[i];

			if (i > 0) {
				r_hint_string += ",";
				name_only_hint_string += ",";
			}

			String enum_field_name = String::utf8(mono_field_get_name(field));
			r_hint_string += enum_field_name;
			name_only_hint_string += enum_field_name;

			// TODO:
			// Instead of using mono_field_get_value_object, we can do this without boxing. Check the
			// internal mono functions: ves_icall_System_Enum_GetEnumValuesAndNames and the get_enum_field.

			MonoObject *val_obj = mono_field_get_value_object(mono_domain_get(), field, NULL);

			ERR_FAIL_NULL_V_MSG(val_obj, -1, "Failed to get '" + enum_field_name + "' constant enum value.");

			bool r_error;
			uint64_t val = GDMonoUtils::unbox_enum_value(val_obj, enum_basetype, r_error);
			ERR_FAIL_COND_V_MSG(r_error, -1, "Failed to unbox '" + enum_field_name + "' constant enum value.");

			unsigned int expected_val = r_hint == PROPERTY_HINT_FLAGS ? 1 << i : i;
			if (val != expected_val) {
				uses_default_values = false;
			}

			r_hint_string += ":";
			r_hint_string += String::num_uint64(val);
		}

		if (uses_default_values) {
			// If we use the format NAME:VAL, that's what the editor displays.
			// That's annoying if the user is not using custom values for the enum constants.
			// This may not be needed in the future if the editor is changed to not display values.
			r_hint_string = name_only_hint_string;
		}
	} else if (p_variant_type == Variant::OBJECT && CACHED_CLASS(GodotResource)->is_assignable_from(p_type.type_class)) {
		GDMonoClass *field_native_class = GDMonoUtils::get_class_native_base(p_type.type_class);
		CRASH_COND(field_native_class == NULL);

		r_hint = PROPERTY_HINT_RESOURCE_TYPE;
		r_hint_string = NATIVE_GDMONOCLASS_NAME(field_native_class);
	} else if (p_allow_generics && p_variant_type == Variant::ARRAY) {
		// Nested arrays are not supported in the inspector

		ManagedType elem_type;

		if (!GDMonoMarshal::try_get_array_element_type(p_type, elem_type))
			return 0;

		Variant::Type elem_variant_type = GDMonoMarshal::managed_to_variant_type(elem_type);

		PropertyHint elem_hint = PROPERTY_HINT_NONE;
		String elem_hint_string;

		ERR_FAIL_COND_V_MSG(elem_variant_type == Variant::NIL, -1, "Unknown array element type.");

		bool preset_hint = false;
		if (elem_variant_type == Variant::STRING) {
			MonoObject *attr = p_member->get_attribute(CACHED_CLASS(ExportAttribute));
			if (PropertyHint(CACHED_FIELD(ExportAttribute, hint)->get_int_value(attr)) == PROPERTY_HINT_ENUM) {
				r_hint_string = itos(elem_variant_type) + "/" + itos(PROPERTY_HINT_ENUM) + ":" + CACHED_FIELD(ExportAttribute, hintString)->get_string_value(attr);
				preset_hint = true;
			}
		}

		if (!preset_hint) {
			int hint_res = _try_get_member_export_hint(p_member, elem_type, elem_variant_type, /* allow_generics: */ false, elem_hint, elem_hint_string);

			ERR_FAIL_COND_V_MSG(hint_res == -1, -1, "Error while trying to determine information about the array element type.");

			// Format: type/hint:hint_string
			r_hint_string = itos(elem_variant_type) + "/" + itos(elem_hint) + ":" + elem_hint_string;
		}

		r_hint = PROPERTY_HINT_TYPE_STRING;

	} else if (p_allow_generics && p_variant_type == Variant::DICTIONARY) {
		// TODO: Dictionaries are not supported in the inspector
	} else {
		return 0;
	}

	return 1;
}
#endif

void CSharpScript::_clear() {
	tool = false;
	valid = false;
	reload_invalidated = true;

	base = NULL;
	native = NULL;
	script_class = NULL;
}

Variant CSharpScript::call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (unlikely(GDMono::get_singleton() == NULL)) {
		// Probably not the best error but eh.
		r_error.error = Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	GD_MONO_SCOPE_THREAD_ATTACH;

	GDMonoClass *top = script_class;

	while (top && top != native) {
		GDMonoMethod *method = top->get_method(p_method, p_argcount);

		if (method && method->is_static()) {
			MonoObject *result = method->invoke(NULL, p_args);

			if (result) {
				return GDMonoMarshal::mono_object_to_variant(result);
			} else {
				return Variant();
			}
		}

		top = top->get_parent_class();
	}

	// No static method found. Try regular instance calls
	return Script::call(p_method, p_args, p_argcount, r_error);
}

void CSharpScript::_resource_path_changed() {
	String path = get_path();

	if (!path.empty()) {
		name = get_path().get_file().get_basename();
	}
}

bool CSharpScript::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == CSharpLanguage::singleton->string_names._script_source) {
		r_ret = get_source_code();
		return true;
	}

	return false;
}

bool CSharpScript::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == CSharpLanguage::singleton->string_names._script_source) {
		set_source_code(p_value);
		reload();
		return true;
	}

	return false;
}

void CSharpScript::_get_property_list(List<PropertyInfo> *p_properties) const {
	p_properties->push_back(PropertyInfo(Variant::STRING, CSharpLanguage::singleton->string_names._script_source, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
}

void CSharpScript::_bind_methods() {
	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "new", &CSharpScript::_new, MethodInfo("new"));
}

Ref<CSharpScript> CSharpScript::create_for_managed_type(GDMonoClass *p_class, GDMonoClass *p_native) {
	// This method should not fail, only assertions allowed

	CRASH_COND(p_class == NULL);

	// TODO OPTIMIZE: Cache the 'CSharpScript' associated with this 'p_class' instead of allocating a new one every time
	Ref<CSharpScript> script = memnew(CSharpScript);

	initialize_for_managed_type(script, p_class, p_native);

	return script;
}

void CSharpScript::initialize_for_managed_type(Ref<CSharpScript> p_script, GDMonoClass *p_class, GDMonoClass *p_native) {
	// This method should not fail, only assertions allowed

	CRASH_COND(p_class == NULL);

	p_script->name = p_class->get_name();
	p_script->script_class = p_class;
	p_script->native = p_native;

	CRASH_COND(p_script->native == NULL);

	GDMonoClass *base = p_script->script_class->get_parent_class();

	if (base != p_script->native)
		p_script->base = base;

	p_script->valid = true;
	p_script->tool = p_script->script_class->has_attribute(CACHED_CLASS(ToolAttribute));
	p_script->reload_invalidated = false;

	if (!p_script->tool) {
		GDMonoClass *nesting_class = p_script->script_class->get_nesting_class();
		p_script->tool = nesting_class && nesting_class->has_attribute(CACHED_CLASS(ToolAttribute));
	}

#ifdef TOOLS_ENABLED
	if (!p_script->tool) {
		p_script->tool = p_script->script_class->get_assembly() == GDMono::get_singleton()->get_tools_assembly();
	}
#endif

#ifdef DEBUG_ENABLED
	// For debug builds, we must fetch from all native base methods as well.
	// Native base methods must be fetched before the current class.
	// Not needed if the script class itself is a native class.

	if (p_script->script_class != p_script->native) {
		GDMonoClass *native_top = p_script->native;
		while (native_top) {
			native_top->fetch_methods_with_godot_api_checks(p_script->native);

			if (native_top == CACHED_CLASS(GodotObject))
				break;

			native_top = native_top->get_parent_class();
		}
	}
#endif

	p_script->script_class->fetch_methods_with_godot_api_checks(p_script->native);

	// Need to fetch method from base classes as well
	GDMonoClass *top = p_script->script_class;
	while (top && top != p_script->native) {
		top->fetch_methods_with_godot_api_checks(p_script->native);
		top = top->get_parent_class();
	}

	p_script->load_script_signals(p_script->script_class, p_script->native);
#ifdef TOOLS_ENABLED
	p_script->_update_member_info_no_exports();
#endif
}

bool CSharpScript::can_instance() const {
#ifdef TOOLS_ENABLED
	bool extra_cond = tool || ScriptServer::is_scripting_enabled();
#else
	bool extra_cond = true;
#endif

	// FIXME Need to think this through better.
	// For tool scripts, this will never fire if the class is not found. That's because we
	// don't know if it's a tool script if we can't find the class to access the attributes.
	if (extra_cond && !script_class) {
		if (GDMono::get_singleton()->get_project_assembly() == NULL) {
			// The project assembly is not loaded
			ERR_FAIL_V_MSG(NULL, "Cannot instance script because the project assembly is not loaded. Script: '" + get_path() + "'.");
		} else {
			// The project assembly is loaded, but the class could not found
			ERR_FAIL_V_MSG(NULL, "Cannot instance script because the class '" + name + "' could not be found. Script: '" + get_path() + "'.");
		}
	}

	return valid && extra_cond;
}

StringName CSharpScript::get_instance_base_type() const {
	if (native)
		return native->get_name();
	else
		return StringName();
}

CSharpInstance *CSharpScript::_create_instance(const Variant **p_args, int p_argcount, Object *p_owner, bool p_isref, Variant::CallError &r_error) {
	GD_MONO_ASSERT_THREAD_ATTACHED;

	/* STEP 1, CREATE */

	// Search the constructor first, to fail with an error if it's not found before allocating anything else.
	GDMonoMethod *ctor = script_class->get_method(CACHED_STRING_NAME(dotctor), p_argcount);
	if (ctor == NULL) {
		ERR_FAIL_COND_V_MSG(p_argcount == 0, NULL,
				"Cannot create script instance. The class '" + script_class->get_full_name() +
						"' does not define a parameterless constructor." +
						(get_path().empty() ? String() : " Path: '" + get_path() + "'."));

		ERR_FAIL_V_MSG(NULL, "Constructor not found.");
	}

	Ref<Reference> ref;
	if (p_isref) {
		// Hold it alive. Important if we have to dispose a script instance binding before creating the CSharpInstance.
		ref = Ref<Reference>(static_cast<Reference *>(p_owner));
	}

	// If the object had a script instance binding, dispose it before adding the CSharpInstance
	if (p_owner->has_script_instance_binding(CSharpLanguage::get_singleton()->get_language_index())) {
		void *data = p_owner->get_script_instance_binding(CSharpLanguage::get_singleton()->get_language_index());
		CRASH_COND(data == NULL);

		CSharpScriptBinding &script_binding = ((Map<Object *, CSharpScriptBinding>::Element *)data)->get();
		if (script_binding.inited && script_binding.gchandle.is_valid()) {
			MonoObject *mono_object = script_binding.gchandle->get_target();
			if (mono_object) {
				MonoException *exc = NULL;
				GDMonoUtils::dispose(mono_object, &exc);

				if (exc) {
					GDMonoUtils::set_pending_exception(exc);
				}
			}

			script_binding.inited = false;
		}
	}

	CSharpInstance *instance = memnew(CSharpInstance);
	instance->base_ref = p_isref;
	instance->script = Ref<CSharpScript>(this);
	instance->owner = p_owner;
	instance->owner->set_script_instance(instance);

	/* STEP 2, INITIALIZE AND CONSTRUCT */

	MonoObject *mono_object = mono_object_new(mono_domain_get(), script_class->get_mono_ptr());

	if (!mono_object) {
		// Important to clear this before destroying the script instance here
		instance->script = Ref<CSharpScript>();
		instance->owner = NULL;

		bool die = instance->_unreference_owner_unsafe();
		// Not ok for the owner to die here. If there is a situation where this can happen, it will be considered a bug.
		CRASH_COND(die);

		p_owner->set_script_instance(NULL);
		r_error.error = Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		ERR_FAIL_V_MSG(NULL, "Failed to allocate memory for the object.");
	}

	// Tie managed to unmanaged
	instance->gchandle = MonoGCHandle::create_strong(mono_object);

	if (instance->base_ref)
		instance->_reference_owner_unsafe(); // Here, after assigning the gchandle (for the refcount_incremented callback)

	{
		MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
		instances.insert(instance->owner);
	}

	CACHED_FIELD(GodotObject, ptr)->set_value_raw(mono_object, instance->owner);

	// Construct
	ctor->invoke(mono_object, p_args);

	/* STEP 3, PARTY */

	//@TODO make thread safe
	return instance;
}

Variant CSharpScript::_new(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (!valid) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	r_error.error = Variant::CallError::CALL_OK;

	ERR_FAIL_NULL_V(native, Variant());

	GD_MONO_SCOPE_THREAD_ATTACH;

	Object *owner = ClassDB::instance(NATIVE_GDMONOCLASS_NAME(native));

	REF ref;
	Reference *r = Object::cast_to<Reference>(owner);
	if (r) {
		ref = REF(r);
	}

	CSharpInstance *instance = _create_instance(p_args, p_argcount, owner, r != NULL, r_error);
	if (!instance) {
		if (ref.is_null()) {
			memdelete(owner); //no owner, sorry
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

	GD_MONO_SCOPE_THREAD_ATTACH;

	if (native) {
		String native_name = NATIVE_GDMONOCLASS_NAME(native);
		if (!ClassDB::is_parent_class(p_this->get_class_name(), native_name)) {
			if (ScriptDebugger::get_singleton()) {
				CSharpLanguage::get_singleton()->debug_break_parse(get_path(), 0, "Script inherits from native type '" + native_name + "', so it can't be instanced in object of type: '" + p_this->get_class() + "'");
			}
			ERR_FAIL_V_MSG(NULL, "Script inherits from native type '" + native_name + "', so it can't be instanced in object of type: '" + p_this->get_class() + "'.");
		}
	}

	Variant::CallError unchecked_error;
	return _create_instance(NULL, 0, p_this, Object::cast_to<Reference>(p_this) != NULL, unchecked_error);
}

PlaceHolderScriptInstance *CSharpScript::placeholder_instance_create(Object *p_this) {
#ifdef TOOLS_ENABLED
	PlaceHolderScriptInstance *si = memnew(PlaceHolderScriptInstance(CSharpLanguage::get_singleton(), Ref<Script>(this), p_this));
	placeholders.insert(si);
	_update_exports(si);
	return si;
#else
	return NULL;
#endif
}

bool CSharpScript::instance_has(const Object *p_this) const {
	MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
	return instances.has((Object *)p_this);
}

bool CSharpScript::has_source_code() const {
	return !source.empty();
}

String CSharpScript::get_source_code() const {
	return source;
}

void CSharpScript::set_source_code(const String &p_code) {
	if (source == p_code)
		return;
	source = p_code;
#ifdef TOOLS_ENABLED
	source_changed_cache = true;
#endif
}

void CSharpScript::get_script_method_list(List<MethodInfo> *p_list) const {
	if (!script_class)
		return;

	GD_MONO_SCOPE_THREAD_ATTACH;

	// TODO: We're filtering out constructors but there may be other methods unsuitable for explicit calls.
	GDMonoClass *top = script_class;

	while (top && top != native) {
		const Vector<GDMonoMethod *> &methods = top->get_all_methods();
		for (int i = 0; i < methods.size(); ++i) {
			MethodInfo minfo = methods[i]->get_method_info();
			if (minfo.name != CACHED_STRING_NAME(dotctor)) {
				p_list->push_back(methods[i]->get_method_info());
			}
		}

		top = top->get_parent_class();
	}
}

bool CSharpScript::has_method(const StringName &p_method) const {
	if (!script_class)
		return false;

	GD_MONO_SCOPE_THREAD_ATTACH;

	return script_class->has_fetched_method_unknown_params(p_method);
}

MethodInfo CSharpScript::get_method_info(const StringName &p_method) const {
	if (!script_class)
		return MethodInfo();

	GD_MONO_SCOPE_THREAD_ATTACH;

	GDMonoClass *top = script_class;

	while (top && top != native) {
		GDMonoMethod *params = top->get_fetched_method_unknown_params(p_method);
		if (params) {
			return params->get_method_info();
		}

		top = top->get_parent_class();
	}

	return MethodInfo();
}

Error CSharpScript::reload(bool p_keep_state) {
	if (!reload_invalidated) {
		return OK;
	}
	// In the case of C#, reload doesn't really do any script reloading.
	// That's done separately via domain reloading.
	reload_invalidated = false;

	GD_MONO_SCOPE_THREAD_ATTACH;

	GDMonoAssembly *project_assembly = GDMono::get_singleton()->get_project_assembly();

	if (project_assembly) {
		const Variant *script_metadata_var = CSharpLanguage::get_singleton()->get_scripts_metadata().getptr(get_path());
		if (script_metadata_var) {
			Dictionary script_metadata = script_metadata_var->operator Dictionary()["class"];
			const Variant *namespace_ = script_metadata.getptr("namespace");
			const Variant *class_name = script_metadata.getptr("class_name");
			ERR_FAIL_NULL_V(namespace_, ERR_BUG);
			ERR_FAIL_NULL_V(class_name, ERR_BUG);
			GDMonoClass *klass = project_assembly->get_class(namespace_->operator String(), class_name->operator String());
			if (klass && CACHED_CLASS(GodotObject)->is_assignable_from(klass)) {
				script_class = klass;
			}
		} else {
			// Missing script metadata. Fallback to legacy method
			script_class = project_assembly->get_object_derived_class(name);
		}

		valid = script_class != NULL;

		if (script_class) {
#ifdef DEBUG_ENABLED
			print_verbose("Found class " + script_class->get_full_name() + " for script " + get_path());
#endif

			tool = script_class->has_attribute(CACHED_CLASS(ToolAttribute));

			if (!tool) {
				GDMonoClass *nesting_class = script_class->get_nesting_class();
				tool = nesting_class && nesting_class->has_attribute(CACHED_CLASS(ToolAttribute));
			}

#ifdef TOOLS_ENABLED
			if (!tool) {
				tool = script_class->get_assembly() == GDMono::get_singleton()->get_tools_assembly();
			}
#endif

			native = GDMonoUtils::get_class_native_base(script_class);

			CRASH_COND(native == NULL);

			GDMonoClass *base_class = script_class->get_parent_class();

			if (base_class != native)
				base = base_class;

#ifdef DEBUG_ENABLED
			// For debug builds, we must fetch from all native base methods as well.
			// Native base methods must be fetched before the current class.
			// Not needed if the script class itself is a native class.

			if (script_class != native) {
				GDMonoClass *native_top = native;
				while (native_top) {
					native_top->fetch_methods_with_godot_api_checks(native);

					if (native_top == CACHED_CLASS(GodotObject))
						break;

					native_top = native_top->get_parent_class();
				}
			}
#endif

			script_class->fetch_methods_with_godot_api_checks(native);

			// Need to fetch method from base classes as well
			GDMonoClass *top = script_class;
			while (top && top != native) {
				top->fetch_methods_with_godot_api_checks(native);
				top = top->get_parent_class();
			}

			load_script_signals(script_class, native);
			_update_exports();
		}

		return OK;
	}

	return ERR_FILE_MISSING_DEPENDENCIES;
}

ScriptLanguage *CSharpScript::get_language() const {
	return CSharpLanguage::get_singleton();
}

bool CSharpScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
#ifdef TOOLS_ENABLED

	const Map<StringName, Variant>::Element *E = exported_members_defval_cache.find(p_property);
	if (E) {
		r_value = E->get();
		return true;
	}

	if (base_cache.is_valid()) {
		return base_cache->get_property_default_value(p_property, r_value);
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
	return _signals.has(p_signal);
}

void CSharpScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	for (const Map<StringName, Vector<Argument>>::Element *E = _signals.front(); E; E = E->next()) {
		MethodInfo mi;

		mi.name = E->key();
		for (int i = 0; i < E->get().size(); i++) {
			PropertyInfo arg;
			arg.name = E->get()[i].name;
			mi.arguments.push_back(arg);
		}
		r_signals->push_back(mi);
	}
}

bool CSharpScript::inherits_script(const Ref<Script> &p_script) const {
	Ref<CSharpScript> cs = p_script;
	if (cs.is_null()) {
		return false;
	}

	if (script_class == nullptr || cs->script_class == nullptr) {
		return false;
	}

	if (script_class == cs->script_class) {
		return true;
	}

	return cs->script_class->is_assignable_from(script_class);
}

Ref<Script> CSharpScript::get_base_script() const {
	// TODO search in metadata file once we have it, not important any way?
	return Ref<Script>();
}

void CSharpScript::get_script_property_list(List<PropertyInfo> *p_list) const {
	List<PropertyInfo> props;

	for (OrderedHashMap<StringName, PropertyInfo>::ConstElement E = member_info.front(); E; E = E.next()) {
		props.push_front(E.value());
	}

	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		p_list->push_back(E->get());
	}
}

int CSharpScript::get_member_line(const StringName &p_member) const {
	// TODO omnisharp
	return -1;
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

StringName CSharpScript::get_script_name() const {
	return name;
}

CSharpScript::CSharpScript() :
		script_list(this) {
	_clear();

#ifdef TOOLS_ENABLED
	source_changed_cache = false;
	placeholder_fallback_enabled = false;
	exports_invalidated = true;
#endif

	signals_invalidated = true;

	_resource_path_changed();

#ifdef DEBUG_ENABLED
	{
		MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
		CSharpLanguage::get_singleton()->script_list.add(&this->script_list);
	}
#endif
}

CSharpScript::~CSharpScript() {
#ifdef DEBUG_ENABLED
	MutexLock lock(CSharpLanguage::get_singleton()->script_instances_mutex);
	CSharpLanguage::get_singleton()->script_list.remove(&this->script_list);
#endif
}

void CSharpScript::get_members(Set<StringName> *p_members) {
#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
	if (p_members) {
		for (Set<StringName>::Element *E = exported_members_names.front(); E; E = E->next()) {
			p_members->insert(E->get());
		}
	}
#endif
}

/*************** RESOURCE ***************/

RES ResourceFormatLoaderCSharpScript::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_no_subresource_cache) {
	if (r_error)
		*r_error = ERR_FILE_CANT_OPEN;

	// TODO ignore anything inside bin/ and obj/ in tools builds?

	CSharpScript *script = memnew(CSharpScript);

	Ref<CSharpScript> scriptres(script);

#if defined(DEBUG_ENABLED) || defined(TOOLS_ENABLED)
	Error err = script->load_source_code(p_path);
	ERR_FAIL_COND_V_MSG(err != OK, RES(), "Cannot load C# script file '" + p_path + "'.");
#endif

	script->set_path(p_original_path);

	script->reload();

	if (r_error)
		*r_error = OK;

	return scriptres;
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

Error ResourceFormatSaverCSharpScript::save(const String &p_path, const RES &p_resource, uint32_t p_flags) {
	Ref<CSharpScript> sqscr = p_resource;
	ERR_FAIL_COND_V(sqscr.is_null(), ERR_INVALID_PARAMETER);

	String source = sqscr->get_source_code();

#ifdef TOOLS_ENABLED
	if (!FileAccess::exists(p_path)) {
		// The file does not yet exists, let's assume the user just created this script

		if (_create_project_solution_if_needed()) {
			CSharpProject::add_item(GodotSharpDirs::get_project_csproj_path(),
					"Compile",
					ProjectSettings::get_singleton()->globalize_path(p_path));
		} else {
			ERR_PRINT("C# project could not be created; cannot add file: '" + p_path + "'.");
		}
	}
#endif

	Error err;
	FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save C# script file '" + p_path + "'.");

	file->store_string(source);

	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		memdelete(file);
		return ERR_CANT_CREATE;
	}

	file->close();
	memdelete(file);

#ifdef TOOLS_ENABLED
	if (ScriptServer::is_reload_scripts_on_save_enabled()) {
		CSharpLanguage::get_singleton()->reload_tool_script(p_resource, false);
	}
#endif

	return OK;
}

void ResourceFormatSaverCSharpScript::get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<CSharpScript>(p_resource.ptr())) {
		p_extensions->push_back("cs");
	}
}

bool ResourceFormatSaverCSharpScript::recognize(const RES &p_resource) const {
	return Object::cast_to<CSharpScript>(p_resource.ptr()) != NULL;
}

CSharpLanguage::StringNameCache::StringNameCache() {
	_signal_callback = StaticCString::create("_signal_callback");
	_set = StaticCString::create("_set");
	_get = StaticCString::create("_get");
	_get_property_list = StaticCString::create("_get_property_list");
	_notification = StaticCString::create("_notification");
	_script_source = StaticCString::create("script/source");
	on_before_serialize = StaticCString::create("OnBeforeSerialize");
	on_after_deserialize = StaticCString::create("OnAfterDeserialize");
	dotctor = StaticCString::create(".ctor");
}
