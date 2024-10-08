/**************************************************************************/
/*  gdextension_library_loader.cpp                                        */
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

#include "gdextension_library_loader.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/version.h"
#include "gdextension.h"

Vector<SharedObject> GDExtensionLibraryLoader::find_extension_dependencies(const String &p_path, Ref<ConfigFile> p_config, std::function<bool(String)> p_has_feature) {
	Vector<SharedObject> dependencies_shared_objects;
	if (p_config->has_section("dependencies")) {
		List<String> config_dependencies;
		p_config->get_section_keys("dependencies", &config_dependencies);

		for (const String &dependency : config_dependencies) {
			Vector<String> dependency_tags = dependency.split(".");
			bool all_tags_met = true;
			for (int i = 0; i < dependency_tags.size(); i++) {
				String tag = dependency_tags[i].strip_edges();
				if (!p_has_feature(tag)) {
					all_tags_met = false;
					break;
				}
			}

			if (all_tags_met) {
				Dictionary dependency_value = p_config->get_value("dependencies", dependency);
				for (const Variant *key = dependency_value.next(nullptr); key; key = dependency_value.next(key)) {
					String dependency_path = *key;
					String target_path = dependency_value[*key];
					if (dependency_path.is_relative_path()) {
						dependency_path = p_path.get_base_dir().path_join(dependency_path);
					}
					dependencies_shared_objects.push_back(SharedObject(dependency_path, dependency_tags, target_path));
				}
				break;
			}
		}
	}

	return dependencies_shared_objects;
}

String GDExtensionLibraryLoader::find_extension_library(const String &p_path, Ref<ConfigFile> p_config, std::function<bool(String)> p_has_feature, PackedStringArray *r_tags) {
	// First, check the explicit libraries.
	if (p_config->has_section("libraries")) {
		List<String> libraries;
		p_config->get_section_keys("libraries", &libraries);

		// Iterate the libraries, finding the best matching tags.
		String best_library_path;
		Vector<String> best_library_tags;
		for (const String &E : libraries) {
			Vector<String> tags = E.split(".");
			bool all_tags_met = true;
			for (int i = 0; i < tags.size(); i++) {
				String tag = tags[i].strip_edges();
				if (!p_has_feature(tag)) {
					all_tags_met = false;
					break;
				}
			}

			if (all_tags_met && tags.size() > best_library_tags.size()) {
				best_library_path = p_config->get_value("libraries", E);
				best_library_tags = tags;
			}
		}

		if (!best_library_path.is_empty()) {
			if (best_library_path.is_relative_path()) {
				best_library_path = p_path.get_base_dir().path_join(best_library_path);
			}
			if (r_tags != nullptr) {
				r_tags->append_array(best_library_tags);
			}
			return best_library_path;
		}
	}

	// Second, try to autodetect.
	String autodetect_library_prefix;
	if (p_config->has_section_key("configuration", "autodetect_library_prefix")) {
		autodetect_library_prefix = p_config->get_value("configuration", "autodetect_library_prefix");
	}
	if (!autodetect_library_prefix.is_empty()) {
		String autodetect_path = autodetect_library_prefix;
		if (autodetect_path.is_relative_path()) {
			autodetect_path = p_path.get_base_dir().path_join(autodetect_path);
		}

		// Find the folder and file parts of the prefix.
		String folder;
		String file_prefix;
		if (DirAccess::dir_exists_absolute(autodetect_path)) {
			folder = autodetect_path;
		} else if (DirAccess::dir_exists_absolute(autodetect_path.get_base_dir())) {
			folder = autodetect_path.get_base_dir();
			file_prefix = autodetect_path.get_file();
		} else {
			ERR_FAIL_V_MSG(String(), vformat("Error in extension: %s. Could not find folder for automatic detection of libraries files. autodetect_library_prefix=\"%s\"", p_path, autodetect_library_prefix));
		}

		// Open the folder.
		Ref<DirAccess> dir = DirAccess::open(folder);
		ERR_FAIL_COND_V_MSG(dir.is_null(), String(), vformat("Error in extension: %s. Could not open folder for automatic detection of libraries files. autodetect_library_prefix=\"%s\"", p_path, autodetect_library_prefix));

		// Iterate the files and check the prefixes, finding the best matching file.
		String best_file;
		Vector<String> best_file_tags;
		dir->list_dir_begin();
		String file_name = dir->_get_next();
		while (file_name != "") {
			if (!dir->current_is_dir() && file_name.begins_with(file_prefix)) {
				// Check if the files matches all requested feature tags.
				String tags_str = file_name.trim_prefix(file_prefix);
				tags_str = tags_str.trim_suffix(tags_str.get_extension());

				Vector<String> tags = tags_str.split(".", false);
				bool all_tags_met = true;
				for (int i = 0; i < tags.size(); i++) {
					String tag = tags[i].strip_edges();
					if (!p_has_feature(tag)) {
						all_tags_met = false;
						break;
					}
				}

				// If all tags are found in the feature list, and we found more tags than before, use this file.
				if (all_tags_met && tags.size() > best_file_tags.size()) {
					best_file_tags = tags;
					best_file = file_name;
				}
			}
			file_name = dir->_get_next();
		}

		if (!best_file.is_empty()) {
			String library_path = folder.path_join(best_file);
			if (r_tags != nullptr) {
				r_tags->append_array(best_file_tags);
			}
			return library_path;
		}
	}
	return String();
}

Error GDExtensionLibraryLoader::open_library(const String &p_path) {
	Error err = parse_gdextension_file(p_path);
	if (err != OK) {
		return err;
	}

	String abs_path = ProjectSettings::get_singleton()->globalize_path(library_path);

	Vector<String> abs_dependencies_paths;
	if (!library_dependencies.is_empty()) {
		for (const SharedObject &dependency : library_dependencies) {
			abs_dependencies_paths.push_back(ProjectSettings::get_singleton()->globalize_path(dependency.path));
		}
	}

	OS::GDExtensionData data = {
		true, // also_set_library_path
		&library_path, // r_resolved_path
		Engine::get_singleton()->is_editor_hint(), // generate_temp_files
		&abs_dependencies_paths, // library_dependencies
	};

	err = OS::get_singleton()->open_dynamic_library(is_static_library ? String() : abs_path, library, &data);
	if (err != OK) {
		return err;
	}

	return OK;
}

Error GDExtensionLibraryLoader::initialize(GDExtensionInterfaceGetProcAddress p_get_proc_address, const Ref<GDExtension> &p_extension, GDExtensionInitialization *r_initialization) {
#ifdef TOOLS_ENABLED
	p_extension->set_reloadable(is_reloadable && Engine::get_singleton()->is_extension_reloading_enabled());
#endif

	for (const KeyValue<String, String> &icon : class_icon_paths) {
		p_extension->class_icon_paths[icon.key] = icon.value;
	}

	void *entry_funcptr = nullptr;

	Error err = OS::get_singleton()->get_dynamic_library_symbol_handle(library, entry_symbol, entry_funcptr, false);

	if (err != OK) {
		ERR_PRINT("GDExtension entry point '" + entry_symbol + "' not found in library " + library_path);
		return err;
	}

	GDExtensionInitializationFunction initialization_function = (GDExtensionInitializationFunction)entry_funcptr;

	GDExtensionBool ret = initialization_function(p_get_proc_address, p_extension.ptr(), r_initialization);

	if (ret) {
		return OK;
	} else {
		ERR_PRINT("GDExtension initialization function '" + entry_symbol + "' returned an error.");
		return FAILED;
	}
}

void GDExtensionLibraryLoader::close_library() {
	OS::get_singleton()->close_dynamic_library(library);
	library = nullptr;
}

bool GDExtensionLibraryLoader::is_library_open() const {
	return library != nullptr;
}

bool GDExtensionLibraryLoader::has_library_changed() const {
#ifdef TOOLS_ENABLED
	// Check only that the last modified time is different (rather than checking
	// that it's newer) since some OS's (namely Windows) will preserve the modified
	// time by default when copying files.
	if (FileAccess::get_modified_time(resource_path) != resource_last_modified_time) {
		return true;
	}
	if (FileAccess::get_modified_time(library_path) != library_last_modified_time) {
		return true;
	}
#endif
	return false;
}

bool GDExtensionLibraryLoader::library_exists() const {
	return FileAccess::exists(resource_path);
}

Error GDExtensionLibraryLoader::parse_gdextension_file(const String &p_path) {
	resource_path = p_path;

	Ref<ConfigFile> config;
	config.instantiate();

	Error err = config->load(p_path);

	if (err != OK) {
		ERR_PRINT("Error loading GDExtension configuration file: " + p_path);
		return err;
	}

	if (!config->has_section_key("configuration", "entry_symbol")) {
		ERR_PRINT("GDExtension configuration file must contain a \"configuration/entry_symbol\" key: " + p_path);
		return ERR_INVALID_DATA;
	}

	entry_symbol = config->get_value("configuration", "entry_symbol");

	uint32_t compatibility_minimum[3] = { 0, 0, 0 };
	if (config->has_section_key("configuration", "compatibility_minimum")) {
		String compat_string = config->get_value("configuration", "compatibility_minimum");
		Vector<int> parts = compat_string.split_ints(".");
		for (int i = 0; i < parts.size(); i++) {
			if (i >= 3) {
				break;
			}
			if (parts[i] >= 0) {
				compatibility_minimum[i] = parts[i];
			}
		}
	} else {
		ERR_PRINT("GDExtension configuration file must contain a \"configuration/compatibility_minimum\" key: " + p_path);
		return ERR_INVALID_DATA;
	}

	if (compatibility_minimum[0] < 4 || (compatibility_minimum[0] == 4 && compatibility_minimum[1] == 0)) {
		ERR_PRINT(vformat("GDExtension's compatibility_minimum (%d.%d.%d) must be at least 4.1.0: %s", compatibility_minimum[0], compatibility_minimum[1], compatibility_minimum[2], p_path));
		return ERR_INVALID_DATA;
	}

	bool compatible = true;
	// Check version lexicographically.
	if (VERSION_MAJOR != compatibility_minimum[0]) {
		compatible = VERSION_MAJOR > compatibility_minimum[0];
	} else if (VERSION_MINOR != compatibility_minimum[1]) {
		compatible = VERSION_MINOR > compatibility_minimum[1];
	} else {
		compatible = VERSION_PATCH >= compatibility_minimum[2];
	}
	if (!compatible) {
		ERR_PRINT(vformat("GDExtension only compatible with Godot version %d.%d.%d or later: %s", compatibility_minimum[0], compatibility_minimum[1], compatibility_minimum[2], p_path));
		return ERR_INVALID_DATA;
	}

	// Optionally check maximum compatibility.
	if (config->has_section_key("configuration", "compatibility_maximum")) {
		uint32_t compatibility_maximum[3] = { 0, 0, 0 };
		String compat_string = config->get_value("configuration", "compatibility_maximum");
		Vector<int> parts = compat_string.split_ints(".");
		for (int i = 0; i < 3; i++) {
			if (i < parts.size() && parts[i] >= 0) {
				compatibility_maximum[i] = parts[i];
			} else {
				// If a version part is missing, set the maximum to an arbitrary high value.
				compatibility_maximum[i] = 9999;
			}
		}

		compatible = true;
		if (VERSION_MAJOR != compatibility_maximum[0]) {
			compatible = VERSION_MAJOR < compatibility_maximum[0];
		} else if (VERSION_MINOR != compatibility_maximum[1]) {
			compatible = VERSION_MINOR < compatibility_maximum[1];
		}
#if VERSION_PATCH
		// #if check to avoid -Wtype-limits warning when 0.
		else {
			compatible = VERSION_PATCH <= compatibility_maximum[2];
		}
#endif

		if (!compatible) {
			ERR_PRINT(vformat("GDExtension only compatible with Godot version %s or earlier: %s", compat_string, p_path));
			return ERR_INVALID_DATA;
		}
	}

	library_path = find_extension_library(p_path, config, [](const String &p_feature) { return OS::get_singleton()->has_feature(p_feature); });

	if (library_path.is_empty()) {
		const String os_arch = OS::get_singleton()->get_name().to_lower() + "." + Engine::get_singleton()->get_architecture_name();
		ERR_PRINT(vformat("No GDExtension library found for current OS and architecture (%s) in configuration file: %s", os_arch, p_path));
		return ERR_FILE_NOT_FOUND;
	}

	is_static_library = library_path.ends_with(".a") || library_path.ends_with(".xcframework");

	if (!library_path.is_resource_file() && !library_path.is_absolute_path()) {
		library_path = p_path.get_base_dir().path_join(library_path);
	}

#ifdef TOOLS_ENABLED
	is_reloadable = config->get_value("configuration", "reloadable", false);

	update_last_modified_time(
			FileAccess::get_modified_time(resource_path),
			FileAccess::get_modified_time(library_path));
#endif

	library_dependencies = find_extension_dependencies(p_path, config, [](const String &p_feature) { return OS::get_singleton()->has_feature(p_feature); });

	// Handle icons if any are specified.
	if (config->has_section("icons")) {
		List<String> keys;
		config->get_section_keys("icons", &keys);
		for (const String &key : keys) {
			String icon_path = config->get_value("icons", key);
			if (icon_path.is_relative_path()) {
				icon_path = p_path.get_base_dir().path_join(icon_path);
			}

			class_icon_paths[key] = icon_path;
		}
	}

	return OK;
}
