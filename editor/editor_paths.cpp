/**************************************************************************/
/*  editor_paths.cpp                                                      */
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

#include "editor_paths.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "main/main.h"

EditorPaths *EditorPaths::singleton = nullptr;

bool EditorPaths::are_paths_valid() const {
	return paths_valid;
}

String EditorPaths::get_data_dir() const {
	return data_dir;
}

String EditorPaths::get_config_dir() const {
	return config_dir;
}

String EditorPaths::get_cache_dir() const {
	return cache_dir;
}

String EditorPaths::get_temp_dir() const {
	return temp_dir;
}

String EditorPaths::get_project_data_dir() const {
	return project_data_dir;
}

bool EditorPaths::is_self_contained() const {
	return self_contained;
}

String EditorPaths::get_self_contained_file() const {
	return self_contained_file;
}

String EditorPaths::get_export_templates_dir() const {
	return get_data_dir().path_join(export_templates_folder);
}

String EditorPaths::get_debug_keystore_path() const {
#ifdef ANDROID_ENABLED
	return "assets://keystores/debug.keystore";
#else
	return get_data_dir().path_join("keystores/debug.keystore");
#endif
}

String EditorPaths::get_project_settings_dir() const {
	return get_project_data_dir().path_join("editor");
}

String EditorPaths::get_text_editor_themes_dir() const {
	return get_config_dir().path_join(text_editor_themes_folder);
}

String EditorPaths::get_script_templates_dir() const {
	return get_config_dir().path_join(script_templates_folder);
}

String EditorPaths::get_project_script_templates_dir() const {
	return GLOBAL_GET("editor/script/templates_search_path");
}

String EditorPaths::get_feature_profiles_dir() const {
	return get_config_dir().path_join(feature_profiles_folder);
}

void EditorPaths::create() {
	memnew(EditorPaths);
}

void EditorPaths::free() {
	ERR_FAIL_NULL(singleton);
	memdelete(singleton);
	singleton = nullptr;
}

void EditorPaths::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_data_dir"), &EditorPaths::get_data_dir);
	ClassDB::bind_method(D_METHOD("get_config_dir"), &EditorPaths::get_config_dir);
	ClassDB::bind_method(D_METHOD("get_cache_dir"), &EditorPaths::get_cache_dir);
	ClassDB::bind_method(D_METHOD("is_self_contained"), &EditorPaths::is_self_contained);
	ClassDB::bind_method(D_METHOD("get_self_contained_file"), &EditorPaths::get_self_contained_file);

	ClassDB::bind_method(D_METHOD("get_project_settings_dir"), &EditorPaths::get_project_settings_dir);
}

EditorPaths::EditorPaths() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;

	project_data_dir = ProjectSettings::get_singleton()->get_project_data_path();

	// Self-contained mode if a `._sc_` or `_sc_` file is present in executable dir.
	String exe_path = OS::get_singleton()->get_executable_path().get_base_dir();
	Ref<DirAccess> d = DirAccess::create_for_path(exe_path);
	if (d->file_exists(exe_path + "/._sc_")) {
		self_contained = true;
		self_contained_file = exe_path + "/._sc_";
	} else if (d->file_exists(exe_path + "/_sc_")) {
		self_contained = true;
		self_contained_file = exe_path + "/_sc_";
	}

	// On macOS, look outside .app bundle, since .app bundle is read-only.
	// Note: This will not work if Gatekeeper path randomization is active.
	if (OS::get_singleton()->has_feature("macos") && exe_path.ends_with("MacOS") && exe_path.path_join("..").simplify_path().ends_with("Contents")) {
		exe_path = exe_path.path_join("../../..").simplify_path();
		d = DirAccess::create_for_path(exe_path);
		if (d->file_exists(exe_path + "/._sc_")) {
			self_contained = true;
			self_contained_file = exe_path + "/._sc_";
		} else if (d->file_exists(exe_path + "/_sc_")) {
			self_contained = true;
			self_contained_file = exe_path + "/_sc_";
		}
	}

	String data_path;
	String config_path;
	String cache_path;

	if (self_contained) {
		// editor is self contained, all in same folder
		data_path = exe_path;
		data_dir = data_path.path_join("editor_data");
		config_path = exe_path;
		config_dir = data_dir;
		cache_path = exe_path;
		cache_dir = data_dir.path_join("cache");
		temp_dir = data_dir.path_join("temp");
	} else {
		// Typically XDG_DATA_HOME or %APPDATA%.
		data_path = OS::get_singleton()->get_data_path();
		data_dir = data_path.path_join(OS::get_singleton()->get_godot_dir_name());
		// Can be different from data_path e.g. on Linux or macOS.
		config_path = OS::get_singleton()->get_config_path();
		config_dir = config_path.path_join(OS::get_singleton()->get_godot_dir_name());
		// Can be different from above paths, otherwise a subfolder of data_dir.
		cache_path = OS::get_singleton()->get_cache_path();
		if (cache_path == data_path) {
			cache_dir = data_dir.path_join("cache");
		} else {
			cache_dir = cache_path.path_join(OS::get_singleton()->get_godot_dir_name());
		}
		temp_dir = OS::get_singleton()->get_temp_path();
	}

	paths_valid = (!data_path.is_empty() && !config_path.is_empty() && !cache_path.is_empty());
	ERR_FAIL_COND_MSG(!paths_valid, "Editor data, config, or cache paths are invalid.");

	// Validate or create each dir and its relevant subdirectories.

	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	// Data dir.
	{
		if (dir->change_dir(data_dir) != OK) {
			dir->make_dir_recursive(data_dir);
			if (dir->change_dir(data_dir) != OK) {
				ERR_PRINT("Could not create editor data directory: " + data_dir);
				paths_valid = false;
			}
		}

		if (!dir->dir_exists(export_templates_folder)) {
			dir->make_dir(export_templates_folder);
		}
	}

	// Config dir.
	{
		if (dir->change_dir(config_dir) != OK) {
			dir->make_dir_recursive(config_dir);
			if (dir->change_dir(config_dir) != OK) {
				ERR_PRINT("Could not create editor config directory: " + config_dir);
				paths_valid = false;
			}
		}

		if (!dir->dir_exists(text_editor_themes_folder)) {
			dir->make_dir(text_editor_themes_folder);
		}
		if (!dir->dir_exists(script_templates_folder)) {
			dir->make_dir(script_templates_folder);
		}
		if (!dir->dir_exists(feature_profiles_folder)) {
			dir->make_dir(feature_profiles_folder);
		}
	}

	// Cache dir.
	{
		if (dir->change_dir(cache_dir) != OK) {
			dir->make_dir_recursive(cache_dir);
			if (dir->change_dir(cache_dir) != OK) {
				ERR_PRINT("Could not create editor cache directory: " + cache_dir);
				paths_valid = false;
			}
		}
	}

	// Validate or create project-specific editor data dir,
	// including shader cache subdir.
	if (Engine::get_singleton()->is_project_manager_hint() || (Main::is_cmdline_tool() && !ProjectSettings::get_singleton()->is_project_loaded())) {
		// Nothing to create, use shared editor data dir for shader cache.
		Engine::get_singleton()->set_shader_cache_path(data_dir);
	} else {
		Ref<DirAccess> dir_res = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (dir_res->change_dir(project_data_dir) != OK) {
			dir_res->make_dir_recursive(project_data_dir);
			if (dir_res->change_dir(project_data_dir) != OK) {
				ERR_PRINT("Could not create project data directory (" + project_data_dir + ") in: " + dir_res->get_current_dir());
				paths_valid = false;
			}
		}

		// Check that the project data directory `.gdignore` file exists.
		String project_data_gdignore_file_path = project_data_dir.path_join(".gdignore");
		if (!FileAccess::exists(project_data_gdignore_file_path)) {
			// Add an empty .gdignore file to avoid scan.
			Ref<FileAccess> f = FileAccess::open(project_data_gdignore_file_path, FileAccess::WRITE);
			if (f.is_valid()) {
				f->store_line("");
			} else {
				ERR_PRINT("Failed to create file " + project_data_gdignore_file_path.quote() + ".");
			}
		}

		Engine::get_singleton()->set_shader_cache_path(project_data_dir);

		// Editor metadata dir.
		if (!dir_res->dir_exists("editor")) {
			dir_res->make_dir("editor");
		}
		// Imported assets dir.
		String imported_files_path = ProjectSettings::get_singleton()->get_imported_files_path();
		if (!dir_res->dir_exists(imported_files_path)) {
			dir_res->make_dir(imported_files_path);
		}
	}
}
