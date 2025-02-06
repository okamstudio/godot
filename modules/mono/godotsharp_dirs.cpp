/**************************************************************************/
/*  godotsharp_dirs.cpp                                                   */
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

#include "godotsharp_dirs.h"

#include "mono_gd/gd_mono.h"
#include "utils/path_utils.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"

#ifdef TOOLS_ENABLED
#include "core/version.h"
#include "editor/editor_paths.h"
#endif

namespace GodotSharpDirs {

String _get_expected_build_config() {
#ifdef TOOLS_ENABLED
	return "Debug";
#else

#ifdef DEBUG_ENABLED
	return "ExportDebug";
#else
	return "ExportRelease";
#endif

#endif
}

String _get_mono_user_dir() {
#ifdef TOOLS_ENABLED
	if (EditorPaths::get_singleton()) {
		return EditorPaths::get_singleton()->get_data_dir().path_join("mono");
	} else {
		String settings_path = OS::get_singleton()->get_data_path().path_join(OS::get_singleton()->get_godot_dir_name());

		// Self-contained mode if a `._sc_` or `_sc_` file is present in executable dir.
		String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
		Ref<DirAccess> d = DirAccess::create_for_path(exe_dir);
		if (d->file_exists("._sc_") || d->file_exists("_sc_")) {
			// contain yourself
			settings_path = exe_dir.path_join("editor_data");
		}

		// On macOS, look outside .app bundle, since .app bundle is read-only.
		// Note: This will not work if Gatekeeper path randomization is active.
		if (OS::get_singleton()->has_feature("macos") && exe_dir.ends_with("MacOS") && exe_dir.path_join("..").simplify_path().ends_with("Contents")) {
			exe_dir = exe_dir.path_join("../../..").simplify_path();
			d = DirAccess::create_for_path(exe_dir);
			if (d->file_exists("._sc_") || d->file_exists("_sc_")) {
				// contain yourself
				settings_path = exe_dir.path_join("editor_data");
			}
		}

		return settings_path.path_join("mono");
	}
#else
	return OS::get_singleton()->get_user_data_dir().path_join("mono");
#endif
}

#if !TOOLS_ENABLED
// This should be the equivalent of GodotTools.Utils.OS.PlatformNameMap.
static const char *platform_name_map[][2] = {
	{ "Windows", "windows" },
	{ "macOS", "macos" },
	{ "Linux", "linuxbsd" },
	{ "FreeBSD", "linuxbsd" },
	{ "NetBSD", "linuxbsd" },
	{ "BSD", "linuxbsd" },
	{ "Android", "android" },
	{ "iOS", "ios" },
	{ "Web", "web" },
	{ nullptr, nullptr }
};

String _get_platform_name() {
	String platform_name = OS::get_singleton()->get_name();

	int idx = 0;
	while (platform_name_map[idx][0] != nullptr) {
		if (platform_name_map[idx][0] == platform_name) {
			return platform_name_map[idx][1];
		}
		idx++;
	}

	return "";
}
#endif

class _GodotSharpDirs {
public:
	String res_metadata_dir;
	String res_temp_assemblies_dir;
	String mono_user_dir;
	String api_assemblies_dir;

#ifdef TOOLS_ENABLED
	String build_logs_dir;
	String data_editor_tools_dir;
#endif

private:
	_GodotSharpDirs() {
		String res_data_dir = ProjectSettings::get_singleton()->get_project_data_path().path_join("mono");
		res_metadata_dir = res_data_dir.path_join("metadata");

		// TODO use paths from csproj
		res_temp_assemblies_dir = res_data_dir.path_join("temp").path_join("bin").path_join(_get_expected_build_config());

#ifdef WEB_ENABLED
		mono_user_dir = "user://";
#else
		mono_user_dir = _get_mono_user_dir();
#endif

		String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
		String res_dir = OS::get_singleton()->get_bundle_resource_dir();

#ifdef TOOLS_ENABLED
		String data_dir_root = exe_dir.path_join("GodotSharp");
		data_editor_tools_dir = data_dir_root.path_join("Tools");
		String api_assemblies_base_dir = data_dir_root.path_join("Api");
		build_logs_dir = mono_user_dir.path_join("build_logs");
#ifdef MACOS_ENABLED
		if (!DirAccess::exists(data_editor_tools_dir)) {
			data_editor_tools_dir = res_dir.path_join("GodotSharp").path_join("Tools");
		}
		if (!DirAccess::exists(api_assemblies_base_dir)) {
			api_assemblies_base_dir = res_dir.path_join("GodotSharp").path_join("Api");
		}
#endif
		api_assemblies_dir = api_assemblies_base_dir.path_join(GDMono::get_expected_api_build_config());
#else // TOOLS_ENABLED
		String platform = _get_platform_name();
		String arch = Engine::get_singleton()->get_architecture_name();
		String appname_safe = path::get_csharp_project_name();
		String packed_path = "res://.godot/mono/publish/" + arch;

		Dictionary dic;
		dic["app_name"] = appname_safe;
		dic["platform"] = platform;
		dic["arch"] = arch;

		String data_dir_name = String(GLOBAL_GET("dotnet/project/data_directory")).format(dic);

		if (DirAccess::exists(packed_path)) {
			// The dotnet publish data is packed in the pck/zip.
			String data_dir_root = OS::get_singleton()->get_cache_path().path_join(data_dir_name);
			bool has_data = false;
			if (!has_data) {
				// 1. Try to access the data directly.
				String global_packed = ProjectSettings::get_singleton()->globalize_path(packed_path);
				if (global_packed.is_absolute_path() && FileAccess::exists(global_packed.path_join(".dotnet-publish-manifest"))) {
					data_dir_root = global_packed;
					has_data = true;
				}
			}
			if (!has_data) {
				// 2. Check if the data was extracted before and is up-to-date.
				String packed_manifest = packed_path.path_join(".dotnet-publish-manifest");
				String extracted_manifest = data_dir_root.path_join(".dotnet-publish-manifest");
				if (FileAccess::exists(packed_manifest) && FileAccess::exists(extracted_manifest)) {
					if (FileAccess::get_file_as_bytes(packed_manifest) == FileAccess::get_file_as_bytes(extracted_manifest)) {
						has_data = true;
					}
				}
			}
			if (!has_data) {
				// 3. Extract the data to a temporary location to load from there, delete old data if it exists but is not up-to-date.
				Ref<DirAccess> da;
				if (DirAccess::exists(data_dir_root)) {
					da = DirAccess::open(data_dir_root);
					ERR_FAIL_COND(da.is_null());
					ERR_FAIL_COND(da->erase_contents_recursive() != OK);
				}
				da = DirAccess::create_for_path(packed_path);
				ERR_FAIL_COND(da.is_null());
				ERR_FAIL_COND(da->copy_dir(packed_path, data_dir_root) != OK);
			}
			api_assemblies_dir = data_dir_root;
		} else {
			// The dotnet publish data is in a directory next to the executable.
			String data_dir_root = exe_dir.path_join(data_dir_name);
#ifdef MACOS_ENABLED
			if (!DirAccess::exists(data_dir_root)) {
				data_dir_root = res_dir.path_join(data_dir_name);
			}
#endif
			api_assemblies_dir = data_dir_root;
		}
#endif
	}

public:
	static _GodotSharpDirs &get_singleton() {
		static _GodotSharpDirs singleton;
		return singleton;
	}
};

String get_res_metadata_dir() {
	return _GodotSharpDirs::get_singleton().res_metadata_dir;
}

String get_res_temp_assemblies_dir() {
	return _GodotSharpDirs::get_singleton().res_temp_assemblies_dir;
}

String get_api_assemblies_dir() {
	return _GodotSharpDirs::get_singleton().api_assemblies_dir;
}

String get_mono_user_dir() {
	return _GodotSharpDirs::get_singleton().mono_user_dir;
}

#ifdef TOOLS_ENABLED
String get_build_logs_dir() {
	return _GodotSharpDirs::get_singleton().build_logs_dir;
}

String get_data_editor_tools_dir() {
	return _GodotSharpDirs::get_singleton().data_editor_tools_dir;
}
#endif

} // namespace GodotSharpDirs
