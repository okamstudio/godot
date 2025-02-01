/**************************************************************************/
/*  godot_plugin_config.h                                                 */
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

#pragma once

#include "core/error/error_list.h"
#include "core/io/config_file.h"
#include "core/string/ustring.h"

/*
 The `config` section and fields are required and defined as follow:
- **name**: name of the plugin
- **binary**: path to static `.a` library
- **use_swift_runtime**: optional boolean field used to determine if Swift runtime is used

The `dependencies` and fields are optional.
- **linked**: dependencies that should only be linked.
- **embedded**: dependencies that should be linked and embedded into application.
- **system**: system dependencies that should be linked.
- **capabilities**: capabilities that would be used for `UIRequiredDeviceCapabilities` options in Info.plist file.
- **files**: files that would be copied into application

The `plist` section are optional.
- **key**: key and value that would be added in Info.plist file.
 */

struct PluginConfigIOS {
	inline static const char *PLUGIN_CONFIG_EXT = ".gdip";

	inline static const char *CONFIG_SECTION = "config";
	inline static const char *CONFIG_NAME_KEY = "name";
	inline static const char *CONFIG_BINARY_KEY = "binary";
	inline static const char *CONFIG_USE_SWIFT_KEY = "use_swift_runtime";
	inline static const char *CONFIG_INITIALIZE_KEY = "initialization";
	inline static const char *CONFIG_DEINITIALIZE_KEY = "deinitialization";

	inline static const char *DEPENDENCIES_SECTION = "dependencies";
	inline static const char *DEPENDENCIES_LINKED_KEY = "linked";
	inline static const char *DEPENDENCIES_EMBEDDED_KEY = "embedded";
	inline static const char *DEPENDENCIES_SYSTEM_KEY = "system";
	inline static const char *DEPENDENCIES_CAPABILITIES_KEY = "capabilities";
	inline static const char *DEPENDENCIES_FILES_KEY = "files";
	inline static const char *DEPENDENCIES_LINKER_FLAGS = "linker_flags";

	inline static const char *PLIST_SECTION = "plist";

	enum PlistItemType {
		UNKNOWN,
		STRING,
		INTEGER,
		BOOLEAN,
		RAW,
		STRING_INPUT,
	};

	struct PlistItem {
		PlistItemType type;
		String value;
	};

	// Set to true when the config file is properly loaded.
	bool valid_config = false;
	bool supports_targets = false;
	// Unix timestamp of last change to this plugin.
	uint64_t last_updated = 0;

	// Required config section
	String name;
	String binary;
	bool use_swift_runtime;
	String initialization_method;
	String deinitialization_method;

	// Optional dependencies section
	Vector<String> linked_dependencies;
	Vector<String> embedded_dependencies;
	Vector<String> system_dependencies;

	Vector<String> files_to_copy;
	Vector<String> capabilities;

	Vector<String> linker_flags;

	// Optional plist section
	// String value is default value.
	// Currently supports `string`, `boolean`, `integer`, `raw`, `string_input` types
	// <name>:<type> = <value>
	HashMap<String, PlistItem> plist;

	static String resolve_local_dependency_path(String plugin_config_dir, String dependency_path);

	static String resolve_system_dependency_path(String dependency_path);

	static Vector<String> resolve_local_dependencies(String plugin_config_dir, Vector<String> p_paths);

	static Vector<String> resolve_system_dependencies(Vector<String> p_paths);

	static bool validate_plugin(PluginConfigIOS &plugin_config);

	static String get_plugin_main_binary(PluginConfigIOS &plugin_config, bool p_debug);

	static uint64_t get_plugin_modification_time(const PluginConfigIOS &plugin_config, const String &config_path);

	static PluginConfigIOS load_plugin_config(Ref<ConfigFile> config_file, const String &path);
};
