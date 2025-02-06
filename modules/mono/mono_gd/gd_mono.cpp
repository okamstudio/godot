/**************************************************************************/
/*  gd_mono.cpp                                                           */
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

#include "gd_mono.h"

#include "../csharp_script.h"
#include "../glue/runtime_interop.h"
#include "../godotsharp_dirs.h"
#include "../thirdparty/coreclr_delegates.h"
#include "../thirdparty/hostfxr.h"
#include "../utils/path_utils.h"
#include "gd_mono_cache.h"

#ifdef TOOLS_ENABLED
#include "../editor/hostfxr_resolver.h"
#endif

#include "core/config/project_settings.h"
#include "core/debugger/engine_debugger.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/os/thread.h"

#ifdef UNIX_ENABLED
#include <dlfcn.h>
#endif

GDMono *GDMono::singleton = nullptr;

namespace {
hostfxr_initialize_for_dotnet_command_line_fn hostfxr_initialize_for_dotnet_command_line = nullptr;
hostfxr_initialize_for_runtime_config_fn hostfxr_initialize_for_runtime_config = nullptr;
hostfxr_get_runtime_delegate_fn hostfxr_get_runtime_delegate = nullptr;
hostfxr_close_fn hostfxr_close = nullptr;

#ifndef TOOLS_ENABLED
typedef int(CORECLR_DELEGATE_CALLTYPE *coreclr_create_delegate_fn)(void *hostHandle, unsigned int domainId, const char *entryPointAssemblyName, const char *entryPointTypeName, const char *entryPointMethodName, void **delegate);
typedef int(CORECLR_DELEGATE_CALLTYPE *coreclr_initialize_fn)(const char *exePath, const char *appDomainFriendlyName, int propertyCount, const char **propertyKeys, const char **propertyValues, void **hostHandle, unsigned int *domainId);

coreclr_create_delegate_fn coreclr_create_delegate = nullptr;
coreclr_initialize_fn coreclr_initialize = nullptr;
#endif

#ifdef _WIN32
static_assert(sizeof(char_t) == sizeof(char16_t));
using HostFxrCharString = Char16String;
#define HOSTFXR_STR(m_str) L##m_str
#else
static_assert(sizeof(char_t) == sizeof(char));
using HostFxrCharString = CharString;
#define HOSTFXR_STR(m_str) m_str
#endif

HostFxrCharString str_to_hostfxr(const String &p_str) {
#ifdef _WIN32
	return p_str.utf16();
#else
	return p_str.utf8();
#endif
}

const char_t *get_data(const HostFxrCharString &p_char_str) {
	return (const char_t *)p_char_str.get_data();
}

String find_hostfxr() {
#ifdef TOOLS_ENABLED
	String dotnet_root;
	String fxr_path;
	if (godotsharp::hostfxr_resolver::try_get_path(dotnet_root, fxr_path)) {
		return fxr_path;
	}

	// hostfxr_resolver doesn't look for dotnet in `PATH`. If it fails, we try to find the dotnet
	// executable in `PATH` here and pass its location as `dotnet_root` to `get_hostfxr_path`.
	String dotnet_exe = path::find_executable("dotnet");

	if (!dotnet_exe.is_empty()) {
		// The file found in PATH may be a symlink
		dotnet_exe = path::abspath(path::realpath(dotnet_exe));

		// TODO:
		// Sometimes, the symlink may not point to the dotnet executable in the dotnet root.
		// That's the case with snaps. The snap install should have been found with the
		// previous `get_hostfxr_path`, but it would still be better to do this properly
		// and use something like `dotnet --list-sdks/runtimes` to find the actual location.
		// This way we could also check if the proper sdk or runtime is installed. This would
		// allow us to fail gracefully and show some helpful information in the editor.

		dotnet_root = dotnet_exe.get_base_dir();
		if (godotsharp::hostfxr_resolver::try_get_path_from_dotnet_root(dotnet_root, fxr_path)) {
			return fxr_path;
		}
	}

	ERR_PRINT(String() + ".NET: One of the dependent libraries is missing. " +
			"Typically when the `hostfxr`, `hostpolicy` or `coreclr` dynamic " +
			"libraries are not present in the expected locations.");

	return String();
#else

#if defined(WINDOWS_ENABLED)
	String probe_path = GodotSharpDirs::get_api_assemblies_dir()
								.path_join("hostfxr.dll");
#elif defined(MACOS_ENABLED)
	String probe_path = GodotSharpDirs::get_api_assemblies_dir()
								.path_join("libhostfxr.dylib");
#elif defined(UNIX_ENABLED)
	String probe_path = GodotSharpDirs::get_api_assemblies_dir()
								.path_join("libhostfxr.so");
#else
#error "Platform not supported (yet?)"
#endif

	if (FileAccess::exists(probe_path)) {
		return probe_path;
	}

	return String();

#endif
}

#ifndef TOOLS_ENABLED
String find_monosgen() {
#if defined(ANDROID_ENABLED)
	// Android includes all native libraries in the libs directory of the APK
	// so we assume it exists and use only the name to dlopen it.
	return "libmonosgen-2.0.so";
#else
#if defined(WINDOWS_ENABLED)
	String probe_path = GodotSharpDirs::get_api_assemblies_dir()
								.path_join("monosgen-2.0.dll");
#elif defined(MACOS_ENABLED)
	String probe_path = GodotSharpDirs::get_api_assemblies_dir()
								.path_join("libmonosgen-2.0.dylib");
#elif defined(UNIX_ENABLED)
	String probe_path = GodotSharpDirs::get_api_assemblies_dir()
								.path_join("libmonosgen-2.0.so");
#else
#error "Platform not supported (yet?)"
#endif

	if (FileAccess::exists(probe_path)) {
		return probe_path;
	}

	return String();
#endif
}

String find_coreclr() {
#if defined(WINDOWS_ENABLED)
	String probe_path = GodotSharpDirs::get_api_assemblies_dir()
								.path_join("coreclr.dll");
#elif defined(MACOS_ENABLED)
	String probe_path = GodotSharpDirs::get_api_assemblies_dir()
								.path_join("libcoreclr.dylib");
#elif defined(UNIX_ENABLED)
	String probe_path = GodotSharpDirs::get_api_assemblies_dir()
								.path_join("libcoreclr.so");
#else
#error "Platform not supported (yet?)"
#endif

	if (FileAccess::exists(probe_path)) {
		return probe_path;
	}

	return String();
}
#endif

bool load_hostfxr(void *&r_hostfxr_dll_handle) {
	String hostfxr_path = find_hostfxr();

	if (hostfxr_path.is_empty()) {
		return false;
	}

	print_verbose("Found hostfxr: " + hostfxr_path);

	Error err = OS::get_singleton()->open_dynamic_library(hostfxr_path, r_hostfxr_dll_handle);

	if (err != OK) {
		return false;
	}

	void *lib = r_hostfxr_dll_handle;

	void *symbol = nullptr;

	err = OS::get_singleton()->get_dynamic_library_symbol_handle(lib, "hostfxr_initialize_for_dotnet_command_line", symbol);
	ERR_FAIL_COND_V(err != OK, false);
	hostfxr_initialize_for_dotnet_command_line = (hostfxr_initialize_for_dotnet_command_line_fn)symbol;

	err = OS::get_singleton()->get_dynamic_library_symbol_handle(lib, "hostfxr_initialize_for_runtime_config", symbol);
	ERR_FAIL_COND_V(err != OK, false);
	hostfxr_initialize_for_runtime_config = (hostfxr_initialize_for_runtime_config_fn)symbol;

	err = OS::get_singleton()->get_dynamic_library_symbol_handle(lib, "hostfxr_get_runtime_delegate", symbol);
	ERR_FAIL_COND_V(err != OK, false);
	hostfxr_get_runtime_delegate = (hostfxr_get_runtime_delegate_fn)symbol;

	err = OS::get_singleton()->get_dynamic_library_symbol_handle(lib, "hostfxr_close", symbol);
	ERR_FAIL_COND_V(err != OK, false);
	hostfxr_close = (hostfxr_close_fn)symbol;

	return (hostfxr_initialize_for_runtime_config &&
			hostfxr_get_runtime_delegate &&
			hostfxr_close);
}

#ifndef TOOLS_ENABLED
bool load_coreclr(void *&r_coreclr_dll_handle) {
	String coreclr_path = find_coreclr();

	bool is_monovm = false;
	if (coreclr_path.is_empty()) {
		// Fallback to MonoVM (should have the same API as CoreCLR).
		coreclr_path = find_monosgen();
		is_monovm = true;
	}

	if (coreclr_path.is_empty()) {
		return false;
	}

	const String coreclr_name = is_monovm ? "monosgen" : "coreclr";
	print_verbose("Found " + coreclr_name + ": " + coreclr_path);

	Error err = OS::get_singleton()->open_dynamic_library(coreclr_path, r_coreclr_dll_handle);

	if (err != OK) {
		return false;
	}

	void *lib = r_coreclr_dll_handle;

	void *symbol = nullptr;

	err = OS::get_singleton()->get_dynamic_library_symbol_handle(lib, "coreclr_initialize", symbol);
	ERR_FAIL_COND_V(err != OK, false);
	coreclr_initialize = (coreclr_initialize_fn)symbol;

	err = OS::get_singleton()->get_dynamic_library_symbol_handle(lib, "coreclr_create_delegate", symbol);
	ERR_FAIL_COND_V(err != OK, false);
	coreclr_create_delegate = (coreclr_create_delegate_fn)symbol;

	return (coreclr_initialize &&
			coreclr_create_delegate);
}
#endif

#ifdef TOOLS_ENABLED
load_assembly_and_get_function_pointer_fn initialize_hostfxr_for_config(const char_t *p_config_path) {
	hostfxr_handle cxt = nullptr;
	int rc = hostfxr_initialize_for_runtime_config(p_config_path, nullptr, &cxt);
	if (rc != 0 || cxt == nullptr) {
		hostfxr_close(cxt);
		ERR_FAIL_V_MSG(nullptr, "hostfxr_initialize_for_runtime_config failed with code: " + itos(rc));
	}

	void *load_assembly_and_get_function_pointer = nullptr;

	rc = hostfxr_get_runtime_delegate(cxt,
			hdt_load_assembly_and_get_function_pointer, &load_assembly_and_get_function_pointer);
	if (rc != 0 || load_assembly_and_get_function_pointer == nullptr) {
		ERR_FAIL_V_MSG(nullptr, "hostfxr_get_runtime_delegate failed with code: " + itos(rc));
	}

	hostfxr_close(cxt);

	return (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer;
}
#else
load_assembly_and_get_function_pointer_fn initialize_hostfxr_self_contained(
		const char_t *p_main_assembly_path) {
	hostfxr_handle cxt = nullptr;

	List<String> cmdline_args = OS::get_singleton()->get_cmdline_args();

	List<HostFxrCharString> argv_store;
	Vector<const char_t *> argv;
	argv.resize(cmdline_args.size() + 1);

	argv.write[0] = p_main_assembly_path;

	int i = 1;
	for (const String &E : cmdline_args) {
		HostFxrCharString &stored = argv_store.push_back(str_to_hostfxr(E))->get();
		argv.write[i] = get_data(stored);
		i++;
	}

	int rc = hostfxr_initialize_for_dotnet_command_line(argv.size(), argv.ptrw(), nullptr, &cxt);
	if (rc != 0 || cxt == nullptr) {
		hostfxr_close(cxt);
		ERR_FAIL_V_MSG(nullptr, "hostfxr_initialize_for_dotnet_command_line failed with code: " + itos(rc));
	}

	void *load_assembly_and_get_function_pointer = nullptr;

	rc = hostfxr_get_runtime_delegate(cxt,
			hdt_load_assembly_and_get_function_pointer, &load_assembly_and_get_function_pointer);
	if (rc != 0 || load_assembly_and_get_function_pointer == nullptr) {
		ERR_FAIL_V_MSG(nullptr, "hostfxr_get_runtime_delegate failed with code: " + itos(rc));
	}

	hostfxr_close(cxt);

	return (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer;
}
#endif

#ifdef TOOLS_ENABLED
using godot_plugins_initialize_fn = bool (*)(void *, bool, gdmono::PluginCallbacks *, GDMonoCache::ManagedCallbacks *, const void **, int32_t);
#else
using godot_plugins_initialize_fn = bool (*)(void *, GDMonoCache::ManagedCallbacks *, const void **, int32_t);
#endif

#ifdef TOOLS_ENABLED
godot_plugins_initialize_fn initialize_hostfxr_and_godot_plugins(bool &r_runtime_initialized) {
	godot_plugins_initialize_fn godot_plugins_initialize = nullptr;

	HostFxrCharString godot_plugins_path = str_to_hostfxr(
			GodotSharpDirs::get_api_assemblies_dir().path_join("GodotPlugins.dll"));

	HostFxrCharString config_path = str_to_hostfxr(
			GodotSharpDirs::get_api_assemblies_dir().path_join("GodotPlugins.runtimeconfig.json"));

	load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer =
			initialize_hostfxr_for_config(get_data(config_path));

	if (load_assembly_and_get_function_pointer == nullptr) {
		// Show a message box to the user to make the problem explicit (and explain a potential crash).
		OS::get_singleton()->alert(TTR("Unable to load .NET runtime, no compatible version was found.\nAttempting to create/edit a project will lead to a crash.\n\nPlease install the .NET SDK 8.0 or later from https://get.dot.net and restart Godot."), TTR("Failed to load .NET runtime"));
		ERR_FAIL_V_MSG(nullptr, ".NET: Failed to load compatible .NET runtime");
	}

	r_runtime_initialized = true;

	print_verbose(".NET: hostfxr initialized");

	int rc = load_assembly_and_get_function_pointer(get_data(godot_plugins_path),
			HOSTFXR_STR("GodotPlugins.Main, GodotPlugins"),
			HOSTFXR_STR("InitializeFromEngine"),
			UNMANAGEDCALLERSONLY_METHOD,
			nullptr,
			(void **)&godot_plugins_initialize);
	ERR_FAIL_COND_V_MSG(rc != 0, nullptr, ".NET: Failed to get GodotPlugins initialization function pointer");

	return godot_plugins_initialize;
}
#else
godot_plugins_initialize_fn initialize_hostfxr_and_godot_plugins(bool &r_runtime_initialized) {
	godot_plugins_initialize_fn godot_plugins_initialize = nullptr;

	String assembly_name = path::get_csharp_project_name();

	HostFxrCharString assembly_path = str_to_hostfxr(GodotSharpDirs::get_api_assemblies_dir()
					.path_join(assembly_name + ".dll"));

	load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer =
			initialize_hostfxr_self_contained(get_data(assembly_path));
	ERR_FAIL_NULL_V(load_assembly_and_get_function_pointer, nullptr);

	r_runtime_initialized = true;

	print_verbose(".NET: hostfxr initialized");

	int rc = load_assembly_and_get_function_pointer(get_data(assembly_path),
			get_data(str_to_hostfxr("GodotPlugins.Game.Main, " + assembly_name)),
			HOSTFXR_STR("InitializeFromGameProject"),
			UNMANAGEDCALLERSONLY_METHOD,
			nullptr,
			(void **)&godot_plugins_initialize);
	ERR_FAIL_COND_V_MSG(rc != 0, nullptr, ".NET: Failed to get GodotPlugins initialization function pointer");

	return godot_plugins_initialize;
}

godot_plugins_initialize_fn try_load_native_aot_library(void *&r_aot_dll_handle) {
	String assembly_name = path::get_csharp_project_name();

#if defined(WINDOWS_ENABLED)
	String native_aot_so_path = GodotSharpDirs::get_api_assemblies_dir().path_join(assembly_name + ".dll");
#elif defined(MACOS_ENABLED) || defined(IOS_ENABLED)
	String native_aot_so_path = GodotSharpDirs::get_api_assemblies_dir().path_join(assembly_name + ".dylib");
#elif defined(UNIX_ENABLED)
	String native_aot_so_path = GodotSharpDirs::get_api_assemblies_dir().path_join(assembly_name + ".so");
#else
#error "Platform not supported (yet?)"
#endif

	Error err = OS::get_singleton()->open_dynamic_library(native_aot_so_path, r_aot_dll_handle);

	if (err != OK) {
		return nullptr;
	}

	void *lib = r_aot_dll_handle;

	void *symbol = nullptr;

	err = OS::get_singleton()->get_dynamic_library_symbol_handle(lib, "godotsharp_game_main_init", symbol);
	ERR_FAIL_COND_V(err != OK, nullptr);
	return (godot_plugins_initialize_fn)symbol;
}
#endif

#ifndef TOOLS_ENABLED
String make_tpa_list() {
	String tpa_list;

#if defined(WINDOWS_ENABLED)
	String separator = ";";
#else
	String separator = ":";
#endif

	String assemblies_dir = GodotSharpDirs::get_api_assemblies_dir();
	PackedStringArray files = DirAccess::get_files_at(assemblies_dir);
	for (const String &file : files) {
		tpa_list += assemblies_dir.path_join(file);
		tpa_list += separator;
	}

	return tpa_list;
}

godot_plugins_initialize_fn initialize_coreclr_and_godot_plugins(bool &r_runtime_initialized) {
	godot_plugins_initialize_fn godot_plugins_initialize = nullptr;

	String assembly_name = path::get_csharp_project_name();

	String tpa_list = make_tpa_list();
	const char *prop_keys[] = { "TRUSTED_PLATFORM_ASSEMBLIES" };
	const char *prop_values[] = { tpa_list.utf8().get_data() };
	int nprops = sizeof(prop_keys) / sizeof(prop_keys[0]);

	void *coreclr_handle = nullptr;
	unsigned int domain_id = 0;
	int rc = coreclr_initialize(nullptr, nullptr, nprops, (const char **)&prop_keys, (const char **)&prop_values, &coreclr_handle, &domain_id);
	ERR_FAIL_COND_V_MSG(rc != 0, nullptr, ".NET: Failed to initialize CoreCLR.");

	r_runtime_initialized = true;

	print_verbose(".NET: CoreCLR initialized");

	coreclr_create_delegate(coreclr_handle, domain_id,
			assembly_name.utf8().get_data(),
			"GodotPlugins.Game.Main",
			"InitializeFromGameProject",
			(void **)&godot_plugins_initialize);
	ERR_FAIL_NULL_V_MSG(godot_plugins_initialize, nullptr, ".NET: Failed to get GodotPlugins initialization function pointer");

	return godot_plugins_initialize;
}
#endif

} // namespace

bool GDMono::should_initialize() {
#ifdef TOOLS_ENABLED
	// The editor always needs to initialize the .NET module for now.
	return true;
#else
	return OS::get_singleton()->has_feature("dotnet");
#endif
}

static bool _on_core_api_assembly_loaded() {
	if (!GDMonoCache::godot_api_cache_updated) {
		return false;
	}

	bool debug;
#ifdef DEBUG_ENABLED
	debug = true;
#else
	debug = false;
#endif

	GDMonoCache::managed_callbacks.GD_OnCoreApiAssemblyLoaded(debug);

	return true;
}

void GDMono::initialize() {
	print_verbose(".NET: Initializing module...");

	_init_godot_api_hashes();

	godot_plugins_initialize_fn godot_plugins_initialize = nullptr;

#if !defined(IOS_ENABLED)
	// Check that the .NET assemblies directory exists before trying to use it.
	if (!DirAccess::exists(GodotSharpDirs::get_api_assemblies_dir())) {
		OS::get_singleton()->alert(vformat(RTR("Unable to find the .NET assemblies directory.\nMake sure the '%s' directory exists and contains the .NET assemblies."), GodotSharpDirs::get_api_assemblies_dir()), RTR(".NET assemblies not found"));
		ERR_FAIL_MSG(".NET: Assemblies not found");
	}
#endif

	if (load_hostfxr(hostfxr_dll_handle)) {
		godot_plugins_initialize = initialize_hostfxr_and_godot_plugins(runtime_initialized);
		ERR_FAIL_NULL(godot_plugins_initialize);
	} else {
#if !defined(TOOLS_ENABLED)
		if (load_coreclr(coreclr_dll_handle)) {
			godot_plugins_initialize = initialize_coreclr_and_godot_plugins(runtime_initialized);
		} else {
			godot_plugins_initialize = try_load_native_aot_library(hostfxr_dll_handle);
			if (godot_plugins_initialize != nullptr) {
				runtime_initialized = true;
			}
		}

		if (godot_plugins_initialize == nullptr) {
			ERR_FAIL_MSG(".NET: Failed to load hostfxr");
		}
#else

		// Show a message box to the user to make the problem explicit (and explain a potential crash).
		OS::get_singleton()->alert(TTR("Unable to load .NET runtime, specifically hostfxr.\nAttempting to create/edit a project will lead to a crash.\n\nPlease install the .NET SDK 8.0 or later from https://get.dot.net and restart Godot."), TTR("Failed to load .NET runtime"));
		ERR_FAIL_MSG(".NET: Failed to load hostfxr");
#endif
	}

	int32_t interop_funcs_size = 0;
	const void **interop_funcs = godotsharp::get_runtime_interop_funcs(interop_funcs_size);

	GDMonoCache::ManagedCallbacks managed_callbacks{};

	void *godot_dll_handle = nullptr;

#if defined(UNIX_ENABLED) && !defined(MACOS_ENABLED) && !defined(IOS_ENABLED)
	// Managed code can access it on its own on other platforms
	godot_dll_handle = dlopen(nullptr, RTLD_NOW);
#endif

#ifdef TOOLS_ENABLED
	gdmono::PluginCallbacks plugin_callbacks_res;
	bool init_ok = godot_plugins_initialize(godot_dll_handle,
			Engine::get_singleton()->is_editor_hint(),
			&plugin_callbacks_res, &managed_callbacks,
			interop_funcs, interop_funcs_size);
	ERR_FAIL_COND_MSG(!init_ok, ".NET: GodotPlugins initialization failed");

	plugin_callbacks = plugin_callbacks_res;
#else
	bool init_ok = godot_plugins_initialize(godot_dll_handle, &managed_callbacks,
			interop_funcs, interop_funcs_size);
	ERR_FAIL_COND_MSG(!init_ok, ".NET: GodotPlugins initialization failed");
#endif

	GDMonoCache::update_godot_api_cache(managed_callbacks);

	print_verbose(".NET: GodotPlugins initialized");

	_on_core_api_assembly_loaded();

#ifdef TOOLS_ENABLED
	_try_load_project_assembly();
#endif

	initialized = true;
}

#ifdef TOOLS_ENABLED
void GDMono::_try_load_project_assembly() {
	if (Engine::get_singleton()->is_project_manager_hint()) {
		return;
	}

	// Load the project's main assembly. This doesn't necessarily need to succeed.
	// The game may not be using .NET at all, or if the project does use .NET and
	// we're running in the editor, it may just happen to be it wasn't built yet.
	if (!_load_project_assembly()) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			print_error(".NET: Failed to load project assembly");
		}
	}
}
#endif

void GDMono::_init_godot_api_hashes() {
#ifdef DEBUG_METHODS_ENABLED
	get_api_core_hash();

#ifdef TOOLS_ENABLED
	get_api_editor_hash();
#endif // TOOLS_ENABLED
#endif // DEBUG_METHODS_ENABLED
}

#ifdef TOOLS_ENABLED
bool GDMono::_load_project_assembly() {
	String assembly_name = path::get_csharp_project_name();

	String assembly_path = GodotSharpDirs::get_res_temp_assemblies_dir()
								   .path_join(assembly_name + ".dll");
	assembly_path = ProjectSettings::get_singleton()->globalize_path(assembly_path);

	if (!FileAccess::exists(assembly_path)) {
		return false;
	}

	String loaded_assembly_path;
	bool success = plugin_callbacks.LoadProjectAssemblyCallback(assembly_path.utf16().get_data(), &loaded_assembly_path);

	if (success) {
		project_assembly_path = loaded_assembly_path.simplify_path();
		project_assembly_modified_time = FileAccess::get_modified_time(loaded_assembly_path);
	}

	return success;
}
#endif

#ifdef GD_MONO_HOT_RELOAD
void GDMono::reload_failure() {
	if (++project_load_failure_count >= (int)GLOBAL_GET("dotnet/project/assembly_reload_attempts")) {
		// After reloading a project has failed n times in a row, update the path and modification time
		// to stop any further attempts at loading this assembly, which probably is never going to work anyways.
		project_load_failure_count = 0;

		ERR_PRINT_ED(".NET: Giving up on assembly reloading. Please restart the editor if unloading was failing.");

		String assembly_name = path::get_csharp_project_name();
		String assembly_path = GodotSharpDirs::get_res_temp_assemblies_dir().path_join(assembly_name + ".dll");
		assembly_path = ProjectSettings::get_singleton()->globalize_path(assembly_path);
		project_assembly_path = assembly_path.simplify_path();
		project_assembly_modified_time = FileAccess::get_modified_time(assembly_path);
	}
}

Error GDMono::reload_project_assemblies() {
	ERR_FAIL_COND_V(!runtime_initialized, ERR_BUG);

	finalizing_scripts_domain = true;

	if (!get_plugin_callbacks().UnloadProjectPluginCallback()) {
		ERR_PRINT_ED(".NET: Failed to unload assemblies. Please check https://github.com/godotengine/godot/issues/78513 for more information.");
		reload_failure();
		return FAILED;
	}

	finalizing_scripts_domain = false;

	// Load the project's main assembly. Here, during hot-reloading, we do
	// consider failing to load the project's main assembly to be an error.
	if (!_load_project_assembly()) {
		ERR_PRINT_ED(".NET: Failed to load project assembly.");
		reload_failure();
		return ERR_CANT_OPEN;
	}

	if (project_load_failure_count > 0) {
		project_load_failure_count = 0;
		ERR_PRINT_ED(".NET: Assembly reloading succeeded after failures.");
	}

	return OK;
}
#endif

GDMono::GDMono() {
	singleton = this;
}

GDMono::~GDMono() {
	finalizing_scripts_domain = true;

	if (hostfxr_dll_handle) {
		OS::get_singleton()->close_dynamic_library(hostfxr_dll_handle);
	}
	if (coreclr_dll_handle) {
		OS::get_singleton()->close_dynamic_library(coreclr_dll_handle);
	}

	finalizing_scripts_domain = false;
	runtime_initialized = false;

	singleton = nullptr;
}

namespace mono_bind {

GodotSharp *GodotSharp::singleton = nullptr;

void GodotSharp::reload_assemblies(bool p_soft_reload) {
#ifdef GD_MONO_HOT_RELOAD
	CRASH_COND(CSharpLanguage::get_singleton() == nullptr);
	// This method may be called more than once with `call_deferred`, so we need to check
	// again if reloading is needed to avoid reloading multiple times unnecessarily.
	if (CSharpLanguage::get_singleton()->is_assembly_reloading_needed()) {
		CSharpLanguage::get_singleton()->reload_assemblies(p_soft_reload);
	}
#endif
}

GodotSharp::GodotSharp() {
	singleton = this;
}

GodotSharp::~GodotSharp() {
	singleton = nullptr;
}

} // namespace mono_bind
