/**************************************************************************/
/*  godot_plugin_config.cpp                                               */
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

#include "godot_plugin_config.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"

String PluginConfigIOS::resolve_local_dependency_path(String plugin_config_dir, String dependency_path) {
	String absolute_path;

	if (dependency_path.is_empty()) {
		return absolute_path;
	}

	if (dependency_path.is_absolute_path()) {
		return dependency_path;
	}

	String res_path = ProjectSettings::get_singleton()->globalize_path("res://");
	absolute_path = plugin_config_dir.path_join(dependency_path);

	return absolute_path.replace(res_path, "res://");
}

String PluginConfigIOS::resolve_system_dependency_path(String dependency_path) {
	String absolute_path;

	if (dependency_path.is_empty()) {
		return absolute_path;
	}

	if (dependency_path.is_absolute_path()) {
		return dependency_path;
	}

	String system_path = "/System/Library/Frameworks";

	return system_path.path_join(dependency_path);
}

Vector<String> PluginConfigIOS::resolve_local_dependencies(String plugin_config_dir, Vector<String> p_paths) {
	Vector<String> paths;

	for (int i = 0; i < p_paths.size(); i++) {
		String path = resolve_local_dependency_path(plugin_config_dir, p_paths[i]);

		if (path.is_empty()) {
			continue;
		}

		paths.push_back(path);
	}

	return paths;
}

Vector<String> PluginConfigIOS::resolve_system_dependencies(Vector<String> p_paths) {
	Vector<String> paths;

	for (int i = 0; i < p_paths.size(); i++) {
		String path = resolve_system_dependency_path(p_paths[i]);

		if (path.is_empty()) {
			continue;
		}

		paths.push_back(path);
	}

	return paths;
}

bool PluginConfigIOS::validate_plugin(PluginConfigIOS &plugin_config) {
	bool valid_name = !plugin_config.name.is_empty();
	bool valid_binary_name = !plugin_config.binary.is_empty();
	bool valid_initialize = !plugin_config.initialization_method.is_empty();
	bool valid_deinitialize = !plugin_config.deinitialization_method.is_empty();

	bool fields_value = valid_name && valid_binary_name && valid_initialize && valid_deinitialize;

	if (!fields_value) {
		return false;
	}

	String plugin_extension = plugin_config.binary.get_extension().to_lower();

	if ((plugin_extension == "a" && FileAccess::exists(plugin_config.binary)) ||
			(plugin_extension == "xcframework" && DirAccess::exists(plugin_config.binary))) {
		plugin_config.valid_config = true;
		plugin_config.supports_targets = false;
	} else {
		String file_path = plugin_config.binary.get_base_dir();
		String file_name = plugin_config.binary.get_basename().get_file();
		String file_extension = plugin_config.binary.get_extension();
		String release_file_name = file_path.path_join(file_name + ".release." + file_extension);
		String debug_file_name = file_path.path_join(file_name + ".debug." + file_extension);

		if ((plugin_extension == "a" && FileAccess::exists(release_file_name) && FileAccess::exists(debug_file_name)) ||
				(plugin_extension == "xcframework" && DirAccess::exists(release_file_name) && DirAccess::exists(debug_file_name))) {
			plugin_config.valid_config = true;
			plugin_config.supports_targets = true;
		}
	}

	return plugin_config.valid_config;
}

String PluginConfigIOS::get_plugin_main_binary(PluginConfigIOS &plugin_config, bool p_debug) {
	if (!plugin_config.supports_targets) {
		return plugin_config.binary;
	}

	String plugin_binary_dir = plugin_config.binary.get_base_dir();
	String plugin_name_prefix = plugin_config.binary.get_basename().get_file();
	String plugin_extension = plugin_config.binary.get_extension();
	String plugin_file = plugin_name_prefix + "." + (p_debug ? "debug" : "release") + "." + plugin_extension;

	return plugin_binary_dir.path_join(plugin_file);
}

uint64_t PluginConfigIOS::get_plugin_modification_time(const PluginConfigIOS &plugin_config, const String &config_path) {
	uint64_t last_updated = FileAccess::get_modified_time(config_path);

	if (!plugin_config.supports_targets) {
		last_updated = MAX(last_updated, FileAccess::get_modified_time(plugin_config.binary));
	} else {
		String file_path = plugin_config.binary.get_base_dir();
		String file_name = plugin_config.binary.get_basename().get_file();
		String plugin_extension = plugin_config.binary.get_extension();
		String release_file_name = file_path.path_join(file_name + ".release." + plugin_extension);
		String debug_file_name = file_path.path_join(file_name + ".debug." + plugin_extension);

		last_updated = MAX(last_updated, FileAccess::get_modified_time(release_file_name));
		last_updated = MAX(last_updated, FileAccess::get_modified_time(debug_file_name));
	}

	return last_updated;
}

PluginConfigIOS PluginConfigIOS::load_plugin_config(Ref<ConfigFile> config_file, const String &path) {
	PluginConfigIOS plugin_config = {};

	if (config_file.is_null()) {
		return plugin_config;
	}

	config_file->clear();

	Error err = config_file->load(path);

	if (err != OK) {
		return plugin_config;
	}

	String config_base_dir = path.get_base_dir();

	plugin_config.name = config_file->get_value(PluginConfigIOS::CONFIG_SECTION, PluginConfigIOS::CONFIG_NAME_KEY, String());
	plugin_config.use_swift_runtime = config_file->get_value(PluginConfigIOS::CONFIG_SECTION, PluginConfigIOS::CONFIG_USE_SWIFT_KEY, false);
	plugin_config.initialization_method = config_file->get_value(PluginConfigIOS::CONFIG_SECTION, PluginConfigIOS::CONFIG_INITIALIZE_KEY, String());
	plugin_config.deinitialization_method = config_file->get_value(PluginConfigIOS::CONFIG_SECTION, PluginConfigIOS::CONFIG_DEINITIALIZE_KEY, String());

	String binary_path = config_file->get_value(PluginConfigIOS::CONFIG_SECTION, PluginConfigIOS::CONFIG_BINARY_KEY, String());
	plugin_config.binary = resolve_local_dependency_path(config_base_dir, binary_path);

	if (config_file->has_section(PluginConfigIOS::DEPENDENCIES_SECTION)) {
		Vector<String> linked_dependencies = config_file->get_value(PluginConfigIOS::DEPENDENCIES_SECTION, PluginConfigIOS::DEPENDENCIES_LINKED_KEY, Vector<String>());
		Vector<String> embedded_dependencies = config_file->get_value(PluginConfigIOS::DEPENDENCIES_SECTION, PluginConfigIOS::DEPENDENCIES_EMBEDDED_KEY, Vector<String>());
		Vector<String> system_dependencies = config_file->get_value(PluginConfigIOS::DEPENDENCIES_SECTION, PluginConfigIOS::DEPENDENCIES_SYSTEM_KEY, Vector<String>());
		Vector<String> files = config_file->get_value(PluginConfigIOS::DEPENDENCIES_SECTION, PluginConfigIOS::DEPENDENCIES_FILES_KEY, Vector<String>());

		plugin_config.linked_dependencies = resolve_local_dependencies(config_base_dir, linked_dependencies);
		plugin_config.embedded_dependencies = resolve_local_dependencies(config_base_dir, embedded_dependencies);
		plugin_config.system_dependencies = resolve_system_dependencies(system_dependencies);

		plugin_config.files_to_copy = resolve_local_dependencies(config_base_dir, files);

		plugin_config.capabilities = config_file->get_value(PluginConfigIOS::DEPENDENCIES_SECTION, PluginConfigIOS::DEPENDENCIES_CAPABILITIES_KEY, Vector<String>());

		plugin_config.linker_flags = config_file->get_value(PluginConfigIOS::DEPENDENCIES_SECTION, PluginConfigIOS::DEPENDENCIES_LINKER_FLAGS, Vector<String>());
	}

	if (config_file->has_section(PluginConfigIOS::PLIST_SECTION)) {
		List<String> keys;
		config_file->get_section_keys(PluginConfigIOS::PLIST_SECTION, &keys);

		for (const String &key : keys) {
			Vector<String> key_components = key.split(":");

			String key_value = "";
			PluginConfigIOS::PlistItemType key_type = PluginConfigIOS::PlistItemType::UNKNOWN;

			if (key_components.size() == 1) {
				key_value = key_components[0];
				key_type = PluginConfigIOS::PlistItemType::STRING;
			} else if (key_components.size() == 2) {
				key_value = key_components[0];

				if (key_components[1].to_lower() == "string") {
					key_type = PluginConfigIOS::PlistItemType::STRING;
				} else if (key_components[1].to_lower() == "integer") {
					key_type = PluginConfigIOS::PlistItemType::INTEGER;
				} else if (key_components[1].to_lower() == "boolean") {
					key_type = PluginConfigIOS::PlistItemType::BOOLEAN;
				} else if (key_components[1].to_lower() == "raw") {
					key_type = PluginConfigIOS::PlistItemType::RAW;
				} else if (key_components[1].to_lower() == "string_input") {
					key_type = PluginConfigIOS::PlistItemType::STRING_INPUT;
				}
			}

			if (key_value.is_empty() || key_type == PluginConfigIOS::PlistItemType::UNKNOWN) {
				continue;
			}

			String value;

			switch (key_type) {
				case PluginConfigIOS::PlistItemType::STRING: {
					String raw_value = config_file->get_value(PluginConfigIOS::PLIST_SECTION, key, String());
					value = "<string>" + raw_value + "</string>";
				} break;
				case PluginConfigIOS::PlistItemType::INTEGER: {
					int raw_value = config_file->get_value(PluginConfigIOS::PLIST_SECTION, key, 0);
					Dictionary value_dictionary;
					String value_format = "<integer>$value</integer>";
					value_dictionary["value"] = raw_value;
					value = value_format.format(value_dictionary, "$_");
				} break;
				case PluginConfigIOS::PlistItemType::BOOLEAN:
					if (config_file->get_value(PluginConfigIOS::PLIST_SECTION, key, false)) {
						value = "<true/>";
					} else {
						value = "<false/>";
					}
					break;
				case PluginConfigIOS::PlistItemType::RAW: {
					String raw_value = config_file->get_value(PluginConfigIOS::PLIST_SECTION, key, String());
					value = raw_value;
				} break;
				case PluginConfigIOS::PlistItemType::STRING_INPUT: {
					String raw_value = config_file->get_value(PluginConfigIOS::PLIST_SECTION, key, String());
					value = raw_value;
				} break;
				default:
					continue;
			}

			plugin_config.plist[key_value] = PluginConfigIOS::PlistItem{ key_type, value };
		}
	}

	if (validate_plugin(plugin_config)) {
		plugin_config.last_updated = get_plugin_modification_time(plugin_config, path);
	}

	return plugin_config;
}
