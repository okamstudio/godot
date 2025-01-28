/**************************************************************************/
/*  project_settings.cpp                                                  */
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

#include "project_settings.h"

#include "core/core_bind.h" // For Compression enum.
#include "core/input/input_map.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/marshalls.h"
#include "core/io/resource_uid.h"
#include "core/object/script_language.h"
#include "core/templates/rb_set.h"
#include "core/variant/typed_array.h"
#include "core/variant/variant_parser.h"
#include "core/version.h"

#ifdef TOOLS_ENABLED
#include "modules/modules_enabled.gen.h" // For mono.
#endif // TOOLS_ENABLED

const String ProjectSettings::PROJECT_DATA_DIR_NAME_SUFFIX = "godot";

ProjectSettings *ProjectSettings::singleton = nullptr;

ProjectSettings *ProjectSettings::get_singleton() {
	return singleton;
}

String ProjectSettings::get_project_data_dir_name() const {
	return project_data_dir_name;
}

String ProjectSettings::get_project_data_path() const {
	return "res://" + get_project_data_dir_name();
}

String ProjectSettings::get_resource_path() const {
	return resource_path;
}

String ProjectSettings::get_imported_files_path() const {
	return get_project_data_path().path_join("imported");
}

#ifdef TOOLS_ENABLED
// Returns the features that a project must have when opened with this build of Godot.
// This is used by the project manager to provide the initial_settings for config/features.
const PackedStringArray ProjectSettings::get_required_features() {
	PackedStringArray features;
	features.append(VERSION_BRANCH);
#ifdef REAL_T_IS_DOUBLE
	features.append("Double Precision");
#endif
	return features;
}

// Returns the features supported by this build of Godot. Includes all required features.
const PackedStringArray ProjectSettings::_get_supported_features() {
	PackedStringArray features = get_required_features();
#ifdef MODULE_MONO_ENABLED
	features.append("C#");
#endif
	// Allow pinning to a specific patch number or build type by marking
	// them as supported. They're only used if the user adds them manually.
	features.append(VERSION_BRANCH "." _MKSTR(VERSION_PATCH));
	features.append(VERSION_FULL_CONFIG);
	features.append(VERSION_FULL_BUILD);

#ifdef RD_ENABLED
	features.append("Forward Plus");
	features.append("Mobile");
#endif

#ifdef GLES3_ENABLED
	features.append("GL Compatibility");
#endif
	return features;
}

// Returns the features that this project needs but this build of Godot lacks.
const PackedStringArray ProjectSettings::get_unsupported_features(const PackedStringArray &p_project_features) {
	PackedStringArray unsupported_features;
	PackedStringArray supported_features = singleton->_get_supported_features();
	for (int i = 0; i < p_project_features.size(); i++) {
		if (!supported_features.has(p_project_features[i])) {
			// Temporary compatibility code to ease upgrade to 4.0 beta 2+.
			if (p_project_features[i].begins_with("Vulkan")) {
				continue;
			}
			unsupported_features.append(p_project_features[i]);
		}
	}
	unsupported_features.sort();
	return unsupported_features;
}

// Returns the features that both this project has and this build of Godot has, ensuring required features exist.
const PackedStringArray ProjectSettings::_trim_to_supported_features(const PackedStringArray &p_project_features) {
	// Remove unsupported features if present.
	PackedStringArray features = PackedStringArray(p_project_features);
	PackedStringArray supported_features = _get_supported_features();
	for (int i = p_project_features.size() - 1; i > -1; i--) {
		if (!supported_features.has(p_project_features[i])) {
			features.remove_at(i);
		}
	}
	// Add required features if not present.
	PackedStringArray required_features = get_required_features();
	for (int i = 0; i < required_features.size(); i++) {
		if (!features.has(required_features[i])) {
			features.append(required_features[i]);
		}
	}
	features.sort();
	return features;
}
#endif // TOOLS_ENABLED

String ProjectSettings::localize_path(const String &p_path) const {
	String path = p_path.simplify_path();

	if (resource_path.is_empty() || (path.is_absolute_path() && !path.begins_with(resource_path))) {
		return path;
	}

	// Check if we have a special path (like res://) or a protocol identifier.
	int p = path.find("://");
	bool found = false;
	if (p > 0) {
		found = true;
		for (int i = 0; i < p; i++) {
			if (!is_ascii_alphanumeric_char(path[i])) {
				found = false;
				break;
			}
		}
	}
	if (found) {
		return path;
	}

	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	if (dir->change_dir(path) == OK) {
		String cwd = dir->get_current_dir();
		cwd = cwd.replace("\\", "/");

		// Ensure that we end with a '/'.
		// This is important to ensure that we do not wrongly localize the resource path
		// in an absolute path that just happens to contain this string but points to a
		// different folder (e.g. "/my/project" as resource_path would be contained in
		// "/my/project_data", even though the latter is not part of res://.
		// `path_join("")` is an easy way to ensure we have a trailing '/'.
		const String res_path = resource_path.path_join("");

		// DirAccess::get_current_dir() is not guaranteed to return a path that with a trailing '/',
		// so we must make sure we have it as well in order to compare with 'res_path'.
		cwd = cwd.path_join("");

		if (!cwd.begins_with(res_path)) {
			return path;
		}

		return cwd.replace_first(res_path, "res://");
	} else {
		int sep = path.rfind_char('/');
		if (sep == -1) {
			return "res://" + path;
		}

		String parent = path.substr(0, sep);

		String plocal = localize_path(parent);
		if (plocal.is_empty()) {
			return "";
		}
		// Only strip the starting '/' from 'path' if its parent ('plocal') ends with '/'
		if (plocal[plocal.length() - 1] == '/') {
			sep += 1;
		}
		return plocal + path.substr(sep, path.size() - sep);
	}
}

void ProjectSettings::set_initial_value(const String &p_name, const Variant &p_value) {
	ERR_FAIL_COND_MSG(!props.has(p_name), vformat("Request for nonexistent project setting: '%s'.", p_name));

	// Duplicate so that if value is array or dictionary, changing the setting will not change the stored initial value.
	props[p_name].initial = p_value.duplicate();
}

void ProjectSettings::set_restart_if_changed(const String &p_name, bool p_restart) {
	ERR_FAIL_COND_MSG(!props.has(p_name), vformat("Request for nonexistent project setting: '%s'.", p_name));
	props[p_name].restart_if_changed = p_restart;
}

void ProjectSettings::set_as_basic(const String &p_name, bool p_basic) {
	ERR_FAIL_COND_MSG(!props.has(p_name), vformat("Request for nonexistent project setting: '%s'.", p_name));
	props[p_name].basic = p_basic;
}

void ProjectSettings::set_as_internal(const String &p_name, bool p_internal) {
	ERR_FAIL_COND_MSG(!props.has(p_name), vformat("Request for nonexistent project setting: '%s'.", p_name));
	props[p_name].internal = p_internal;
}

void ProjectSettings::set_ignore_value_in_docs(const String &p_name, bool p_ignore) {
	ERR_FAIL_COND_MSG(!props.has(p_name), vformat("Request for nonexistent project setting: '%s'.", p_name));
#ifdef DEBUG_METHODS_ENABLED
	props[p_name].ignore_value_in_docs = p_ignore;
#endif
}

bool ProjectSettings::get_ignore_value_in_docs(const String &p_name) const {
	ERR_FAIL_COND_V_MSG(!props.has(p_name), false, vformat("Request for nonexistent project setting: '%s'.", p_name));
#ifdef DEBUG_METHODS_ENABLED
	return props[p_name].ignore_value_in_docs;
#else
	return false;
#endif
}

void ProjectSettings::add_hidden_prefix(const String &p_prefix) {
	ERR_FAIL_COND_MSG(hidden_prefixes.has(p_prefix), vformat("Hidden prefix '%s' already exists.", p_prefix));
	hidden_prefixes.push_back(p_prefix);
}

String ProjectSettings::globalize_path(const String &p_path) const {
	if (p_path.begins_with("res://")) {
		if (!resource_path.is_empty()) {
			return p_path.replace("res:/", resource_path);
		}
		return p_path.replace("res://", "");
	} else if (p_path.begins_with("uid://")) {
		const String path = ResourceUID::uid_to_path(p_path);
		if (!resource_path.is_empty()) {
			return path.replace("res:/", resource_path);
		}
		return path.replace("res://", "");
	} else if (p_path.begins_with("user://")) {
		String data_dir = OS::get_singleton()->get_user_data_dir();
		if (!data_dir.is_empty()) {
			return p_path.replace("user:/", data_dir);
		}
		return p_path.replace("user://", "");
	}

	return p_path;
}

bool ProjectSettings::_set(const StringName &p_name, const Variant &p_value) {
	_THREAD_SAFE_METHOD_

	if (p_value.get_type() == Variant::NIL) {
		props.erase(p_name);
		if (p_name.operator String().begins_with("autoload/")) {
			String node_name = p_name.operator String().split("/")[1];
			if (autoloads.has(node_name)) {
				remove_autoload(node_name);
			}
		} else if (p_name.operator String().begins_with("global_group/")) {
			String group_name = p_name.operator String().get_slice("/", 1);
			if (global_groups.has(group_name)) {
				remove_global_group(group_name);
			}
		}
	} else {
		if (p_name == CoreStringName(_custom_features)) {
			Vector<String> custom_feature_array = String(p_value).split(",");
			for (int i = 0; i < custom_feature_array.size(); i++) {
				custom_features.insert(custom_feature_array[i]);
			}
			_queue_changed();
			return true;
		}

		{ // Feature overrides.
			int dot = p_name.operator String().find_char('.');
			if (dot != -1) {
				Vector<String> s = p_name.operator String().split(".");

				for (int i = 1; i < s.size(); i++) {
					String feature = s[i].strip_edges();
					Pair<StringName, StringName> feature_override(feature, p_name);

					if (!feature_overrides.has(s[0])) {
						feature_overrides[s[0]] = LocalVector<Pair<StringName, StringName>>();
					}

					feature_overrides[s[0]].push_back(feature_override);
				}
			}
		}

		if (props.has(p_name)) {
			props[p_name].variant = p_value;
		} else {
			props[p_name] = VariantContainer(p_value, last_order++);
		}
		if (p_name.operator String().begins_with("autoload/")) {
			String node_name = p_name.operator String().split("/")[1];
			AutoloadInfo autoload;
			autoload.name = node_name;
			String path = p_value;
			if (path.begins_with("*")) {
				autoload.is_singleton = true;
				autoload.path = path.substr(1).simplify_path();
			} else {
				autoload.path = path.simplify_path();
			}
			add_autoload(autoload);
		} else if (p_name.operator String().begins_with("global_group/")) {
			String group_name = p_name.operator String().get_slice("/", 1);
			add_global_group(group_name, p_value);
		}
	}

	_queue_changed();
	return true;
}

bool ProjectSettings::_get(const StringName &p_name, Variant &r_ret) const {
	_THREAD_SAFE_METHOD_

	if (!props.has(p_name)) {
		return false;
	}
	r_ret = props[p_name].variant;
	return true;
}

Variant ProjectSettings::get_setting_with_override(const StringName &p_name) const {
	_THREAD_SAFE_METHOD_

	StringName name = p_name;
	if (feature_overrides.has(name)) {
		const LocalVector<Pair<StringName, StringName>> &overrides = feature_overrides[name];
		for (uint32_t i = 0; i < overrides.size(); i++) {
			if (OS::get_singleton()->has_feature(overrides[i].first)) { // Custom features are checked in OS.has_feature() already. No need to check twice.
				if (props.has(overrides[i].second)) {
					name = overrides[i].second;
					break;
				}
			}
		}
	}

	if (!props.has(name)) {
		WARN_PRINT(vformat("Property not found: '%s'.", String(name)));
		return Variant();
	}
	return props[name].variant;
}

struct _VCSort {
	String name;
	Variant::Type type = Variant::VARIANT_MAX;
	int order = 0;
	uint32_t flags = 0;

	bool operator<(const _VCSort &p_vcs) const { return order == p_vcs.order ? name < p_vcs.name : order < p_vcs.order; }
};

void ProjectSettings::_get_property_list(List<PropertyInfo> *p_list) const {
	_THREAD_SAFE_METHOD_

	RBSet<_VCSort> vclist;

	for (const KeyValue<StringName, VariantContainer> &E : props) {
		const VariantContainer *v = &E.value;

		if (v->hide_from_editor) {
			continue;
		}

		_VCSort vc;
		vc.name = E.key;
		vc.order = v->order;
		vc.type = v->variant.get_type();

		bool internal = v->internal;
		if (!internal) {
			for (const String &F : hidden_prefixes) {
				if (vc.name.begins_with(F)) {
					internal = true;
					break;
				}
			}
		}

		if (internal) {
			vc.flags = PROPERTY_USAGE_STORAGE;
		} else {
			vc.flags = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
		}

		if (v->internal) {
			vc.flags |= PROPERTY_USAGE_INTERNAL;
		}

		if (v->basic) {
			vc.flags |= PROPERTY_USAGE_EDITOR_BASIC_SETTING;
		}

		if (v->restart_if_changed) {
			vc.flags |= PROPERTY_USAGE_RESTART_IF_CHANGED;
		}
		vclist.insert(vc);
	}

	for (const _VCSort &E : vclist) {
		String prop_info_name = E.name;
		int dot = prop_info_name.find_char('.');
		if (dot != -1 && !custom_prop_info.has(prop_info_name)) {
			prop_info_name = prop_info_name.substr(0, dot);
		}

		if (custom_prop_info.has(prop_info_name)) {
			PropertyInfo pi = custom_prop_info[prop_info_name];
			pi.name = E.name;
			pi.usage = E.flags;
			p_list->push_back(pi);
		} else {
			p_list->push_back(PropertyInfo(E.type, E.name, PROPERTY_HINT_NONE, "", E.flags));
		}
	}
}

void ProjectSettings::_queue_changed() {
	if (is_changed || !MessageQueue::get_singleton() || MessageQueue::get_singleton()->get_max_buffer_usage() == 0) {
		return;
	}
	is_changed = true;
	callable_mp(this, &ProjectSettings::_emit_changed).call_deferred();
}

void ProjectSettings::_emit_changed() {
	if (!is_changed) {
		return;
	}
	is_changed = false;
	emit_signal("settings_changed");
}

bool ProjectSettings::load_resource_pack(const String &p_pack, bool p_replace_files, int p_offset) {
	return ProjectSettings::_load_resource_pack(p_pack, p_replace_files, p_offset, false);
}

bool ProjectSettings::_load_resource_pack(const String &p_pack, bool p_replace_files, int p_offset, bool p_main_pack) {
	if (PackedData::get_singleton()->is_disabled()) {
		return false;
	}

	if (p_pack == "res://") {
		// Loading the resource directory as a pack source is reserved for internal use only.
		return false;
	}

	if (!p_main_pack && !using_datapack && !OS::get_singleton()->get_resource_dir().is_empty()) {
		// Add the project's resource file system to PackedData so directory access keeps working when
		// the game is running without a main pack, like in the editor or on Android.
		PackedData::get_singleton()->add_pack_source(memnew(PackedSourceDirectory));
		PackedData::get_singleton()->add_pack("res://", false, 0);
		DirAccess::make_default<DirAccessPack>(DirAccess::ACCESS_RESOURCES);
		using_datapack = true;
	}

	bool ok = PackedData::get_singleton()->add_pack(p_pack, p_replace_files, p_offset) == OK;
	if (!ok) {
		return false;
	}

	if (project_loaded) {
		// This pack may have declared new global classes (make sure they are picked up).
		refresh_global_class_list();

		// This pack may have defined new UIDs, make sure they are cached.
		ResourceUID::get_singleton()->load_from_cache(false);
	}

	// If the data pack was found, all directory access will be from here.
	if (!using_datapack) {
		DirAccess::make_default<DirAccessPack>(DirAccess::ACCESS_RESOURCES);
		using_datapack = true;
	}

	return true;
}

void ProjectSettings::_convert_to_last_version(int p_from_version) {
#ifndef DISABLE_DEPRECATED
	if (p_from_version <= 3) {
		// Converts the actions from array to dictionary (array of events to dictionary with deadzone + events)
		for (KeyValue<StringName, ProjectSettings::VariantContainer> &E : props) {
			Variant value = E.value.variant;
			if (String(E.key).begins_with("input/") && value.get_type() == Variant::ARRAY) {
				Array array = value;
				Dictionary action;
				action["deadzone"] = Variant(0.5f);
				action["events"] = array;
				E.value.variant = action;
			}
		}
	}
	if (p_from_version == 5) {
		// Converts the device in events from -3 to -1.
		// -3 was introduced in GH-97707 as a way to prevent a clash in device IDs, but as reported in GH-99243, this leads to problems.
		// -3 was used during dev-releases, so this conversion helps to revert such affected projects.
		// This conversion doesn't affect any other projects, since -3 is not used otherwise.
		for (KeyValue<StringName, ProjectSettings::VariantContainer> &E : props) {
			if (String(E.key).begins_with("input/")) {
				Dictionary action = E.value.variant;
				Array events = action["events"];
				for (int i = 0; i < events.size(); i++) {
					Ref<InputEvent> ev = events[i];
					if (ev.is_valid() && ev->get_device() == -3) {
						ev->set_device(-1);
					}
				}
			}
		}
	}
#endif // DISABLE_DEPRECATED
}

/*
 * This method is responsible for loading a project.godot file and/or data file
 * using the following merit order:
 *  - If using NetworkClient, try to lookup project file or fail.
 *  - If --main-pack was passed by the user (`p_main_pack`), load it or fail.
 *  - Search for project PCKs automatically. For each step we try loading a potential
 *    PCK, and if it doesn't work, we proceed to the next step. If any step succeeds,
 *    we try loading the project settings, and abort if it fails. Steps:
 *    o Bundled PCK in the executable.
 *    o [macOS only] PCK with same basename as the binary in the .app resource dir.
 *    o PCK with same basename as the binary in the binary's directory. We handle both
 *      changing the extension to '.pck' (e.g. 'win_game.exe' -> 'win_game.pck') and
 *      appending '.pck' to the binary name (e.g. 'linux_game' -> 'linux_game.pck').
 *    o PCK with the same basename as the binary in the current working directory.
 *      Same as above for the two possible PCK file names.
 *  - On relevant platforms (Android/iOS), lookup project file in OS resource path.
 *    If found, load it or fail.
 *  - Lookup project file in passed `p_path` (--path passed by the user), i.e. we
 *    are running from source code.
 *    If not found and `p_upwards` is true (--upwards passed by the user), look for
 *    project files in parent folders up to the system root (used to run a game
 *    from command line while in a subfolder).
 *    If a project file is found, load it or fail.
 *    If nothing was found, error out.
 */
Error ProjectSettings::_setup(const String &p_path, const String &p_main_pack, bool p_upwards, bool p_ignore_override) {
	if (!OS::get_singleton()->get_resource_dir().is_empty()) {
		// OS will call ProjectSettings->get_resource_path which will be empty if not overridden!
		// If the OS would rather use a specific location, then it will not be empty.
		resource_path = OS::get_singleton()->get_resource_dir().replace("\\", "/");
		if (!resource_path.is_empty() && resource_path[resource_path.length() - 1] == '/') {
			resource_path = resource_path.substr(0, resource_path.length() - 1); // Chop end.
		}
	}

	// Attempt with a user-defined main pack first

	if (!p_main_pack.is_empty()) {
		bool ok = _load_resource_pack(p_main_pack, false, 0, true);
		ERR_FAIL_COND_V_MSG(!ok, ERR_CANT_OPEN, vformat("Cannot open resource pack '%s'.", p_main_pack));

		Error err = _load_settings_text_or_binary("res://project.godot", "res://project.binary");
		if (err == OK && !p_ignore_override) {
			// Load override from location of the main pack
			// Optional, we don't mind if it fails
			_load_settings_text(p_main_pack.get_base_dir().path_join("override.cfg"));
		}
		return err;
	}

	String exec_path = OS::get_singleton()->get_executable_path();

	if (!exec_path.is_empty()) {
		// We do several tests sequentially until one succeeds to find a PCK,
		// and if so, we attempt loading it at the end.

		// Attempt with PCK bundled into executable.
		bool found = _load_resource_pack(exec_path, false, 0, true);

		// Attempt with exec_name.pck.
		// (This is the usual case when distributing a Godot game.)
		String exec_dir = exec_path.get_base_dir();
		String exec_filename = exec_path.get_file();
		String exec_basename = exec_filename.get_basename();

		// Based on the OS, it can be the exec path + '.pck' (Linux w/o extension, macOS in .app bundle)
		// or the exec path's basename + '.pck' (Windows).
		// We need to test both possibilities as extensions for Linux binaries are optional
		// (so both 'mygame.bin' and 'mygame' should be able to find 'mygame.pck').

#ifdef MACOS_ENABLED
		if (!found) {
			// Attempt to load PCK from macOS .app bundle resources.
			found = _load_resource_pack(OS::get_singleton()->get_bundle_resource_dir().path_join(exec_basename + ".pck"), false, 0, true) || _load_resource_pack(OS::get_singleton()->get_bundle_resource_dir().path_join(exec_filename + ".pck"), false, 0, true);
		}
#endif

		if (!found) {
			// Try to load data pack at the location of the executable.
			// As mentioned above, we have two potential names to attempt.
			found = _load_resource_pack(exec_dir.path_join(exec_basename + ".pck"), false, 0, true) || _load_resource_pack(exec_dir.path_join(exec_filename + ".pck"), false, 0, true);
		}

		if (!found) {
			// If we couldn't find them next to the executable, we attempt
			// the current working directory. Same story, two tests.
			found = _load_resource_pack(exec_basename + ".pck", false, 0, true) || _load_resource_pack(exec_filename + ".pck", false, 0, true);
		}

		// If we opened our package, try and load our project.
		if (found) {
			Error err = _load_settings_text_or_binary("res://project.godot", "res://project.binary");
			if (err == OK && !p_ignore_override) {
				// Load overrides from the PCK and the executable location.
				// Optional, we don't mind if either fails.
				_load_settings_text("res://override.cfg");
				_load_settings_text(exec_path.get_base_dir().path_join("override.cfg"));
			}
			return err;
		}
	}

	// Try to use the filesystem for files, according to OS.
	// (Only Android -when reading from pck- and iOS use this.)

	if (!OS::get_singleton()->get_resource_dir().is_empty()) {
		Error err = _load_settings_text_or_binary("res://project.godot", "res://project.binary");
		if (err == OK && !p_ignore_override) {
			// Optional, we don't mind if it fails.
			_load_settings_text("res://override.cfg");
		}
		return err;
	}

#ifdef MACOS_ENABLED
	// Attempt to load project file from macOS .app bundle resources.
	resource_path = OS::get_singleton()->get_bundle_resource_dir();
	if (!resource_path.is_empty()) {
		if (resource_path[resource_path.length() - 1] == '/') {
			resource_path = resource_path.substr(0, resource_path.length() - 1); // Chop end.
		}
		Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		ERR_FAIL_COND_V_MSG(d.is_null(), ERR_CANT_CREATE, vformat("Cannot create DirAccess for path '%s'.", resource_path));
		d->change_dir(resource_path);

		Error err;

		err = _load_settings_text_or_binary(resource_path.path_join("project.godot"), resource_path.path_join("project.binary"));
		if (err == OK && !p_ignore_override) {
			// Optional, we don't mind if it fails.
			_load_settings_text(resource_path.path_join("override.cfg"));
			return err;
		}
	}
#endif

	// Nothing was found, try to find a project file in provided path (`p_path`)
	// or, if requested (`p_upwards`) in parent directories.

	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V_MSG(d.is_null(), ERR_CANT_CREATE, vformat("Cannot create DirAccess for path '%s'.", p_path));
	d->change_dir(p_path);

	String current_dir = d->get_current_dir();
	bool found = false;
	Error err;

	while (true) {
		// Set the resource path early so things can be resolved when loading.
		resource_path = current_dir;
		resource_path = resource_path.replace("\\", "/"); // Windows path to Unix path just in case.
		err = _load_settings_text_or_binary(current_dir.path_join("project.godot"), current_dir.path_join("project.binary"));
		if (err == OK && !p_ignore_override) {
			// Optional, we don't mind if it fails.
			_load_settings_text(current_dir.path_join("override.cfg"));
			found = true;
			break;
		}

		if (p_upwards) {
			// Try to load settings ascending through parent directories
			d->change_dir("..");
			if (d->get_current_dir() == current_dir) {
				break; // not doing anything useful
			}
			current_dir = d->get_current_dir();
		} else {
			break;
		}
	}

	if (!found) {
		return err;
	}

	if (resource_path.length() && resource_path[resource_path.length() - 1] == '/') {
		resource_path = resource_path.substr(0, resource_path.length() - 1); // Chop end.
	}

	return OK;
}

Error ProjectSettings::setup(const String &p_path, const String &p_main_pack, bool p_upwards, bool p_ignore_override) {
	Error err = _setup(p_path, p_main_pack, p_upwards, p_ignore_override);
	if (err == OK && !p_ignore_override) {
		String custom_settings = GLOBAL_GET("application/config/project_settings_override");
		if (!custom_settings.is_empty()) {
			_load_settings_text(custom_settings);
		}
	}

	// Updating the default value after the project settings have loaded.
	bool use_hidden_directory = GLOBAL_GET("application/config/use_hidden_project_data_directory");
	project_data_dir_name = (use_hidden_directory ? "." : "") + PROJECT_DATA_DIR_NAME_SUFFIX;

	// Using GLOBAL_GET on every block for compressing can be slow, so assigning here.
	Compression::zstd_long_distance_matching = GLOBAL_GET("compression/formats/zstd/long_distance_matching");
	Compression::zstd_level = GLOBAL_GET("compression/formats/zstd/compression_level");
	Compression::zstd_window_log_size = GLOBAL_GET("compression/formats/zstd/window_log_size");

	Compression::zlib_level = GLOBAL_GET("compression/formats/zlib/compression_level");

	Compression::gzip_level = GLOBAL_GET("compression/formats/gzip/compression_level");

	load_scene_groups_cache();

	project_loaded = err == OK;
	return err;
}

bool ProjectSettings::has_setting(const String &p_var) const {
	_THREAD_SAFE_METHOD_

	return props.has(p_var);
}

Error ProjectSettings::_load_settings_binary(const String &p_path) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err != OK) {
		return err;
	}

	uint8_t hdr[4];
	f->get_buffer(hdr, 4);
	ERR_FAIL_COND_V_MSG((hdr[0] != 'E' || hdr[1] != 'C' || hdr[2] != 'F' || hdr[3] != 'G'), ERR_FILE_CORRUPT, "Corrupted header in binary project.binary (not ECFG).");

	uint32_t count = f->get_32();

	for (uint32_t i = 0; i < count; i++) {
		uint32_t slen = f->get_32();
		CharString cs;
		cs.resize(slen + 1);
		cs[slen] = 0;
		f->get_buffer((uint8_t *)cs.ptr(), slen);
		String key;
		key.parse_utf8(cs.ptr(), slen);

		uint32_t vlen = f->get_32();
		Vector<uint8_t> d;
		d.resize(vlen);
		f->get_buffer(d.ptrw(), vlen);
		Variant value;
		err = decode_variant(value, d.ptr(), d.size(), nullptr, true);
		ERR_CONTINUE_MSG(err != OK, vformat("Error decoding property: '%s'.", key));
		set(key, value);
	}

	return OK;
}

Error ProjectSettings::_load_settings_text(const String &p_path) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	if (f.is_null()) {
		// FIXME: Above 'err' error code is ERR_FILE_CANT_OPEN if the file is missing
		// This needs to be streamlined if we want decent error reporting
		return ERR_FILE_NOT_FOUND;
	}

	VariantParser::StreamFile stream;
	stream.f = f;

	String assign;
	Variant value;
	VariantParser::Tag next_tag;

	int lines = 0;
	String error_text;
	String section;
	int config_version = 0;

	while (true) {
		assign = Variant();
		next_tag.fields.clear();
		next_tag.name = String();

		err = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, nullptr, true);
		if (err == ERR_FILE_EOF) {
			// If we're loading a project.godot from source code, we can operate some
			// ProjectSettings conversions if need be.
			_convert_to_last_version(config_version);
			last_save_time = FileAccess::get_modified_time(get_resource_path().path_join("project.godot"));
			return OK;
		}
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Error parsing '%s' at line %d: %s File might be corrupted.", p_path, lines, error_text));

		if (!assign.is_empty()) {
			if (section.is_empty() && assign == "config_version") {
				config_version = value;
				ERR_FAIL_COND_V_MSG(config_version > CONFIG_VERSION, ERR_FILE_CANT_OPEN, vformat("Can't open project at '%s', its `config_version` (%d) is from a more recent and incompatible version of the engine. Expected config version: %d.", p_path, config_version, CONFIG_VERSION));
			} else {
				if (section.is_empty()) {
					set(assign, value);
				} else {
					set(section + "/" + assign, value);
				}
			}
		} else if (!next_tag.name.is_empty()) {
			section = next_tag.name;
		}
	}
}

Error ProjectSettings::_load_settings_text_or_binary(const String &p_text_path, const String &p_bin_path) {
	// Attempt first to load the binary project.godot file.
	Error err = _load_settings_binary(p_bin_path);
	if (err == OK) {
		return OK;
	} else if (err != ERR_FILE_NOT_FOUND) {
		// If the file exists but can't be loaded, we want to know it.
		ERR_PRINT(vformat("Couldn't load file '%s', error code %d.", p_bin_path, err));
	}

	// Fallback to text-based project.godot file if binary was not found.
	err = _load_settings_text(p_text_path);
	if (err == OK) {
		return OK;
	} else if (err != ERR_FILE_NOT_FOUND) {
		ERR_PRINT(vformat("Couldn't load file '%s', error code %d.", p_text_path, err));
	}

	return err;
}

Error ProjectSettings::load_custom(const String &p_path) {
	if (p_path.ends_with(".binary")) {
		return _load_settings_binary(p_path);
	}
	return _load_settings_text(p_path);
}

int ProjectSettings::get_order(const String &p_name) const {
	ERR_FAIL_COND_V_MSG(!props.has(p_name), -1, vformat("Request for nonexistent project setting: '%s'.", p_name));
	return props[p_name].order;
}

void ProjectSettings::set_order(const String &p_name, int p_order) {
	ERR_FAIL_COND_MSG(!props.has(p_name), vformat("Request for nonexistent project setting: '%s'.", p_name));
	props[p_name].order = p_order;
}

void ProjectSettings::set_builtin_order(const String &p_name) {
	ERR_FAIL_COND_MSG(!props.has(p_name), vformat("Request for nonexistent project setting: '%s'.", p_name));
	if (props[p_name].order >= NO_BUILTIN_ORDER_BASE) {
		props[p_name].order = last_builtin_order++;
	}
}

bool ProjectSettings::is_builtin_setting(const String &p_name) const {
	// Return true because a false negative is worse than a false positive.
	ERR_FAIL_COND_V_MSG(!props.has(p_name), true, vformat("Request for nonexistent project setting: '%s'.", p_name));
	return props[p_name].order < NO_BUILTIN_ORDER_BASE;
}

void ProjectSettings::clear(const String &p_name) {
	ERR_FAIL_COND_MSG(!props.has(p_name), vformat("Request for nonexistent project setting: '%s'.", p_name));
	props.erase(p_name);
}

Error ProjectSettings::save() {
	Error error = save_custom(get_resource_path().path_join("project.godot"));
	if (error == OK) {
		last_save_time = FileAccess::get_modified_time(get_resource_path().path_join("project.godot"));
	}
	return error;
}

Error ProjectSettings::_save_settings_binary(const String &p_file, const RBMap<String, List<String>> &p_props, const CustomMap &p_custom, const String &p_custom_features) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Couldn't save project.binary at '%s'.", p_file));

	uint8_t hdr[4] = { 'E', 'C', 'F', 'G' };
	file->store_buffer(hdr, 4);

	int count = 0;

	for (const KeyValue<String, List<String>> &E : p_props) {
		count += E.value.size();
	}

	if (!p_custom_features.is_empty()) {
		// Store how many properties are saved, add one for custom features, which must always go first.
		file->store_32(uint32_t(count + 1));
		String key = CoreStringName(_custom_features);
		file->store_pascal_string(key);

		int len;
		err = encode_variant(p_custom_features, nullptr, len, false);
		ERR_FAIL_COND_V(err != OK, err);

		Vector<uint8_t> buff;
		buff.resize(len);

		err = encode_variant(p_custom_features, buff.ptrw(), len, false);
		ERR_FAIL_COND_V(err != OK, err);
		file->store_32(uint32_t(len));
		file->store_buffer(buff.ptr(), buff.size());

	} else {
		// Store how many properties are saved.
		file->store_32(uint32_t(count));
	}

	for (const KeyValue<String, List<String>> &E : p_props) {
		for (const String &key : E.value) {
			String k = key;
			if (!E.key.is_empty()) {
				k = E.key + "/" + k;
			}
			Variant value;
			if (p_custom.has(k)) {
				value = p_custom[k];
			} else {
				value = get(k);
			}

			file->store_pascal_string(k);

			int len;
			err = encode_variant(value, nullptr, len, true);
			ERR_FAIL_COND_V_MSG(err != OK, ERR_INVALID_DATA, "Error when trying to encode Variant.");

			Vector<uint8_t> buff;
			buff.resize(len);

			err = encode_variant(value, buff.ptrw(), len, true);
			ERR_FAIL_COND_V_MSG(err != OK, ERR_INVALID_DATA, "Error when trying to encode Variant.");
			file->store_32(uint32_t(len));
			file->store_buffer(buff.ptr(), buff.size());
		}
	}

	return OK;
}

Error ProjectSettings::_save_settings_text(const String &p_file, const RBMap<String, List<String>> &p_props, const CustomMap &p_custom, const String &p_custom_features) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::WRITE, &err);

	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Couldn't save project.godot - %s.", p_file));

	file->store_line("; Engine configuration file.");
	file->store_line("; It's best edited using the editor UI and not directly,");
	file->store_line("; since the parameters that go here are not all obvious.");
	file->store_line(";");
	file->store_line("; Format:");
	file->store_line(";   [section] ; section goes between []");
	file->store_line(";   param=value ; assign values to parameters");
	file->store_line("");

	file->store_string("config_version=" + itos(CONFIG_VERSION) + "\n");
	if (!p_custom_features.is_empty()) {
		file->store_string("custom_features=\"" + p_custom_features + "\"\n");
	}
	file->store_string("\n");

	for (const KeyValue<String, List<String>> &E : p_props) {
		if (E.key != p_props.begin()->key) {
			file->store_string("\n");
		}

		if (!E.key.is_empty()) {
			file->store_string("[" + E.key + "]\n\n");
		}
		for (const String &F : E.value) {
			String key = F;
			if (!E.key.is_empty()) {
				key = E.key + "/" + key;
			}
			Variant value;
			if (p_custom.has(key)) {
				value = p_custom[key];
			} else {
				value = get(key);
			}

			String vstr;
			VariantWriter::write_to_string(value, vstr);
			file->store_string(F.property_name_encode() + "=" + vstr + "\n");
		}
	}

	return OK;
}

Error ProjectSettings::_save_custom_bnd(const String &p_file) { // add other params as dictionary and array?
	return save_custom(p_file);
}

#ifdef TOOLS_ENABLED
bool _csproj_exists(const String &p_root_dir) {
	Ref<DirAccess> dir = DirAccess::open(p_root_dir);
	ERR_FAIL_COND_V(dir.is_null(), false);

	dir->list_dir_begin();
	String file_name = dir->_get_next();
	while (file_name != "") {
		if (!dir->current_is_dir() && file_name.get_extension() == "csproj") {
			return true;
		}
		file_name = dir->_get_next();
	}

	return false;
}
#endif // TOOLS_ENABLED

Error ProjectSettings::save_custom(const String &p_path, const CustomMap &p_custom, const Vector<String> &p_custom_features, bool p_merge_with_current) {
	ERR_FAIL_COND_V_MSG(p_path.is_empty(), ERR_INVALID_PARAMETER, "Project settings save path cannot be empty.");

#ifdef TOOLS_ENABLED
	PackedStringArray project_features = get_setting("application/config/features");
	// If there is no feature list currently present, force one to generate.
	if (project_features.is_empty()) {
		project_features = ProjectSettings::get_required_features();
	}
	// Check the rendering API.
	const String rendering_api = has_setting("rendering/renderer/rendering_method") ? (String)get_setting("rendering/renderer/rendering_method") : String();
	if (!rendering_api.is_empty()) {
		// Add the rendering API as a project feature if it doesn't already exist.
		if (!project_features.has(rendering_api)) {
			project_features.append(rendering_api);
		}
	}
	// Check for the existence of a csproj file.
	if (_csproj_exists(get_resource_path())) {
		// If there is a csproj file, add the C# feature if it doesn't already exist.
		if (!project_features.has("C#")) {
			project_features.append("C#");
		}
	} else {
		// If there isn't a csproj file, remove the C# feature if it exists.
		if (project_features.has("C#")) {
			project_features.remove_at(project_features.find("C#"));
		}
	}
	project_features = _trim_to_supported_features(project_features);
	set_setting("application/config/features", project_features);
#endif // TOOLS_ENABLED

	RBSet<_VCSort> vclist;

	if (p_merge_with_current) {
		for (const KeyValue<StringName, VariantContainer> &G : props) {
			const VariantContainer *v = &G.value;

			if (v->hide_from_editor) {
				continue;
			}

			if (p_custom.has(G.key)) {
				continue;
			}

			_VCSort vc;
			vc.name = G.key; //*k;
			vc.order = v->order;
			vc.type = v->variant.get_type();
			vc.flags = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
			if (v->variant == v->initial) {
				continue;
			}

			vclist.insert(vc);
		}
	}

	for (const KeyValue<String, Variant> &E : p_custom) {
		// Lookup global prop to store in the same order
		RBMap<StringName, VariantContainer>::Iterator global_prop = props.find(E.key);

		_VCSort vc;
		vc.name = E.key;
		vc.order = global_prop ? global_prop->value.order : 0xFFFFFFF;
		vc.type = E.value.get_type();
		vc.flags = PROPERTY_USAGE_STORAGE;
		vclist.insert(vc);
	}

	RBMap<String, List<String>> save_props;

	for (const _VCSort &E : vclist) {
		String category = E.name;
		String name = E.name;

		int div = category.find_char('/');

		if (div < 0) {
			category = "";
		} else {
			category = category.substr(0, div);
			name = name.substr(div + 1, name.size());
		}
		save_props[category].push_back(name);
	}

	String save_features;

	for (int i = 0; i < p_custom_features.size(); i++) {
		if (i > 0) {
			save_features += ",";
		}

		String f = p_custom_features[i].strip_edges().replace("\"", "");
		save_features += f;
	}

	if (p_path.ends_with(".godot") || p_path.ends_with("override.cfg")) {
		return _save_settings_text(p_path, save_props, p_custom, save_features);
	} else if (p_path.ends_with(".binary")) {
		return _save_settings_binary(p_path, save_props, p_custom, save_features);
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, vformat("Unknown config file format: '%s'.", p_path));
	}
}

Variant _GLOBAL_DEF(const String &p_var, const Variant &p_default, bool p_restart_if_changed, bool p_ignore_value_in_docs, bool p_basic, bool p_internal) {
	Variant ret;
	if (!ProjectSettings::get_singleton()->has_setting(p_var)) {
		ProjectSettings::get_singleton()->set(p_var, p_default);
	}
	ret = GLOBAL_GET(p_var);

	ProjectSettings::get_singleton()->set_initial_value(p_var, p_default);
	ProjectSettings::get_singleton()->set_builtin_order(p_var);
	ProjectSettings::get_singleton()->set_as_basic(p_var, p_basic);
	ProjectSettings::get_singleton()->set_restart_if_changed(p_var, p_restart_if_changed);
	ProjectSettings::get_singleton()->set_ignore_value_in_docs(p_var, p_ignore_value_in_docs);
	ProjectSettings::get_singleton()->set_as_internal(p_var, p_internal);
	return ret;
}

Variant _GLOBAL_DEF(const PropertyInfo &p_info, const Variant &p_default, bool p_restart_if_changed, bool p_ignore_value_in_docs, bool p_basic, bool p_internal) {
	Variant ret = _GLOBAL_DEF(p_info.name, p_default, p_restart_if_changed, p_ignore_value_in_docs, p_basic, p_internal);
	ProjectSettings::get_singleton()->set_custom_property_info(p_info);
	return ret;
}

void ProjectSettings::_add_property_info_bind(const Dictionary &p_info) {
	ERR_FAIL_COND(!p_info.has("name"));
	ERR_FAIL_COND(!p_info.has("type"));

	PropertyInfo pinfo;
	pinfo.name = p_info["name"];
	ERR_FAIL_COND(!props.has(pinfo.name));
	pinfo.type = Variant::Type(p_info["type"].operator int());
	ERR_FAIL_INDEX(pinfo.type, Variant::VARIANT_MAX);

	if (p_info.has("hint")) {
		pinfo.hint = PropertyHint(p_info["hint"].operator int());
	}
	if (p_info.has("hint_string")) {
		pinfo.hint_string = p_info["hint_string"];
	}

	set_custom_property_info(pinfo);
}

void ProjectSettings::set_custom_property_info(const PropertyInfo &p_info) {
	const String &prop_name = p_info.name;
	ERR_FAIL_COND(!props.has(prop_name));
	custom_prop_info[prop_name] = p_info;
}

const HashMap<StringName, PropertyInfo> &ProjectSettings::get_custom_property_info() const {
	return custom_prop_info;
}

bool ProjectSettings::is_using_datapack() const {
	return using_datapack;
}

bool ProjectSettings::is_project_loaded() const {
	return project_loaded;
}

bool ProjectSettings::_property_can_revert(const StringName &p_name) const {
	return props.has(p_name);
}

bool ProjectSettings::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	const RBMap<StringName, ProjectSettings::VariantContainer>::Element *value = props.find(p_name);
	if (value) {
		r_property = value->value().initial.duplicate();
		return true;
	}
	return false;
}

void ProjectSettings::set_setting(const String &p_setting, const Variant &p_value) {
	set(p_setting, p_value);
}

Variant ProjectSettings::get_setting(const String &p_setting, const Variant &p_default_value) const {
	if (has_setting(p_setting)) {
		return get(p_setting);
	} else {
		return p_default_value;
	}
}

void ProjectSettings::refresh_global_class_list() {
	// This is called after mounting a new PCK file to pick up class changes.
	is_global_class_list_loaded = false; // Make sure we read from the freshly mounted PCK.
	Array script_classes = get_global_class_list();
	for (int i = 0; i < script_classes.size(); i++) {
		Dictionary c = script_classes[i];
		if (!c.has("class") || !c.has("language") || !c.has("path") || !c.has("base")) {
			continue;
		}
		ScriptServer::add_global_class(c["class"], c["base"], c["language"], c["path"]);
	}
}

TypedArray<Dictionary> ProjectSettings::get_global_class_list() {
	if (is_global_class_list_loaded) {
		return global_class_list;
	}

	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load(get_global_class_list_path()) == OK) {
		global_class_list = cf->get_value("", "list", Array());
	} else {
#ifndef TOOLS_ENABLED
		// Script classes can't be recreated in exported project, so print an error.
		ERR_PRINT("Could not load global script cache.");
#endif
	}

	// File read succeeded or failed. If it failed, assume everything is still okay.
	// We will later receive updated class data in store_global_class_list().
	is_global_class_list_loaded = true;

	return global_class_list;
}

String ProjectSettings::get_global_class_list_path() const {
	return get_project_data_path().path_join("global_script_class_cache.cfg");
}

void ProjectSettings::store_global_class_list(const Array &p_classes) {
	Ref<ConfigFile> cf;
	cf.instantiate();
	cf->set_value("", "list", p_classes);
	cf->save(get_global_class_list_path());

	global_class_list = p_classes;
}

bool ProjectSettings::has_custom_feature(const String &p_feature) const {
	return custom_features.has(p_feature);
}

const HashMap<StringName, ProjectSettings::AutoloadInfo> &ProjectSettings::get_autoload_list() const {
	return autoloads;
}

void ProjectSettings::add_autoload(const AutoloadInfo &p_autoload) {
	ERR_FAIL_COND_MSG(p_autoload.name == StringName(), "Trying to add autoload with no name.");
	autoloads[p_autoload.name] = p_autoload;
}

void ProjectSettings::remove_autoload(const StringName &p_autoload) {
	ERR_FAIL_COND_MSG(!autoloads.has(p_autoload), "Trying to remove non-existent autoload.");
	autoloads.erase(p_autoload);
}

bool ProjectSettings::has_autoload(const StringName &p_autoload) const {
	return autoloads.has(p_autoload);
}

ProjectSettings::AutoloadInfo ProjectSettings::get_autoload(const StringName &p_name) const {
	ERR_FAIL_COND_V_MSG(!autoloads.has(p_name), AutoloadInfo(), "Trying to get non-existent autoload.");
	return autoloads[p_name];
}

const HashMap<StringName, String> &ProjectSettings::get_global_groups_list() const {
	return global_groups;
}

void ProjectSettings::add_global_group(const StringName &p_name, const String &p_description) {
	ERR_FAIL_COND_MSG(p_name == StringName(), "Trying to add global group with no name.");
	global_groups[p_name] = p_description;
}

void ProjectSettings::remove_global_group(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!global_groups.has(p_name), "Trying to remove non-existent global group.");
	global_groups.erase(p_name);
}

bool ProjectSettings::has_global_group(const StringName &p_name) const {
	return global_groups.has(p_name);
}

void ProjectSettings::remove_scene_groups_cache(const StringName &p_path) {
	scene_groups_cache.erase(p_path);
}

void ProjectSettings::add_scene_groups_cache(const StringName &p_path, const HashSet<StringName> &p_cache) {
	scene_groups_cache[p_path] = p_cache;
}

void ProjectSettings::save_scene_groups_cache() {
	Ref<ConfigFile> cf;
	cf.instantiate();
	for (const KeyValue<StringName, HashSet<StringName>> &E : scene_groups_cache) {
		if (E.value.is_empty()) {
			continue;
		}
		Array list;
		for (const StringName &group : E.value) {
			list.push_back(group);
		}
		cf->set_value(E.key, "groups", list);
	}
	cf->save(get_scene_groups_cache_path());
}

String ProjectSettings::get_scene_groups_cache_path() const {
	return get_project_data_path().path_join("scene_groups_cache.cfg");
}

void ProjectSettings::load_scene_groups_cache() {
	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load(get_scene_groups_cache_path()) == OK) {
		List<String> scene_paths;
		cf->get_sections(&scene_paths);
		for (const String &E : scene_paths) {
			Array scene_groups = cf->get_value(E, "groups", Array());
			HashSet<StringName> cache;
			for (const Variant &scene_group : scene_groups) {
				cache.insert(scene_group);
			}
			add_scene_groups_cache(E, cache);
		}
	}
}

const HashMap<StringName, HashSet<StringName>> &ProjectSettings::get_scene_groups_cache() const {
	return scene_groups_cache;
}

#ifdef TOOLS_ENABLED
void ProjectSettings::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0) {
		if (pf == "has_setting" || pf == "set_setting" || pf == "get_setting" || pf == "get_setting_with_override" ||
				pf == "set_order" || pf == "get_order" || pf == "set_initial_value" || pf == "set_as_basic" ||
				pf == "set_as_internal" || pf == "set_restart_if_changed" || pf == "clear") {
			for (const KeyValue<StringName, VariantContainer> &E : props) {
				if (E.value.hide_from_editor) {
					continue;
				}

				r_options->push_back(String(E.key).quote());
			}
		}
	}
	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif

void ProjectSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_setting", "name"), &ProjectSettings::has_setting);
	ClassDB::bind_method(D_METHOD("set_setting", "name", "value"), &ProjectSettings::set_setting);
	ClassDB::bind_method(D_METHOD("get_setting", "name", "default_value"), &ProjectSettings::get_setting, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_setting_with_override", "name"), &ProjectSettings::get_setting_with_override);
	ClassDB::bind_method(D_METHOD("get_global_class_list"), &ProjectSettings::get_global_class_list);
	ClassDB::bind_method(D_METHOD("set_order", "name", "position"), &ProjectSettings::set_order);
	ClassDB::bind_method(D_METHOD("get_order", "name"), &ProjectSettings::get_order);
	ClassDB::bind_method(D_METHOD("set_initial_value", "name", "value"), &ProjectSettings::set_initial_value);
	ClassDB::bind_method(D_METHOD("set_as_basic", "name", "basic"), &ProjectSettings::set_as_basic);
	ClassDB::bind_method(D_METHOD("set_as_internal", "name", "internal"), &ProjectSettings::set_as_internal);
	ClassDB::bind_method(D_METHOD("add_property_info", "hint"), &ProjectSettings::_add_property_info_bind);
	ClassDB::bind_method(D_METHOD("set_restart_if_changed", "name", "restart"), &ProjectSettings::set_restart_if_changed);
	ClassDB::bind_method(D_METHOD("clear", "name"), &ProjectSettings::clear);
	ClassDB::bind_method(D_METHOD("localize_path", "path"), &ProjectSettings::localize_path);
	ClassDB::bind_method(D_METHOD("globalize_path", "path"), &ProjectSettings::globalize_path);
	ClassDB::bind_method(D_METHOD("save"), &ProjectSettings::save);
	ClassDB::bind_method(D_METHOD("load_resource_pack", "pack", "replace_files", "offset"), &ProjectSettings::load_resource_pack, DEFVAL(true), DEFVAL(0));

	ClassDB::bind_method(D_METHOD("save_custom", "file"), &ProjectSettings::_save_custom_bnd);

	ADD_SIGNAL(MethodInfo("settings_changed"));
}

void ProjectSettings::_add_builtin_input_map() {
	if (InputMap::get_singleton()) {
		HashMap<String, List<Ref<InputEvent>>> builtins = InputMap::get_singleton()->get_builtins();

		for (KeyValue<String, List<Ref<InputEvent>>> &E : builtins) {
			Array events;

			// Convert list of input events into array
			for (List<Ref<InputEvent>>::Element *I = E.value.front(); I; I = I->next()) {
				events.push_back(I->get());
			}

			Dictionary action;
			action["deadzone"] = Variant(InputMap::DEFAULT_DEADZONE);
			action["events"] = events;

			String action_name = "input/" + E.key;
			GLOBAL_DEF(action_name, action);
			input_presets.push_back(action_name);
		}
	}
}

ProjectSettings::ProjectSettings() {
	// Initialization of engine variables should be done in the setup() method,
	// so that the values can be overridden from project.godot or project.binary.

	CRASH_COND_MSG(singleton != nullptr, "Instantiating a new ProjectSettings singleton is not supported.");
	singleton = this;

#ifdef TOOLS_ENABLED
	// Available only at runtime in editor builds. Needs to be processed before anything else to work properly.
	if (!Engine::get_singleton()->is_editor_hint()) {
		String editor_features = OS::get_singleton()->get_environment("GODOT_EDITOR_CUSTOM_FEATURES");
		if (!editor_features.is_empty()) {
			PackedStringArray feature_list = editor_features.split(",");
			for (const String &s : feature_list) {
				custom_features.insert(s);
			}
		}
	}
#endif

	GLOBAL_DEF_BASIC("application/config/name", "");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::DICTIONARY, "application/config/name_localized", PROPERTY_HINT_LOCALIZABLE_STRING), Dictionary());
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "application/config/description", PROPERTY_HINT_MULTILINE_TEXT), "");
	GLOBAL_DEF_BASIC("application/config/version", "");
	GLOBAL_DEF_INTERNAL(PropertyInfo(Variant::STRING, "application/config/tags"), PackedStringArray());
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "application/run/main_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn,*.res"), "");
	GLOBAL_DEF("application/run/disable_stdout", false);
	GLOBAL_DEF("application/run/disable_stderr", false);
	GLOBAL_DEF("application/run/print_header", true);
	GLOBAL_DEF("application/run/enable_alt_space_menu", false);
	GLOBAL_DEF_RST("application/config/use_hidden_project_data_directory", true);
	GLOBAL_DEF("application/config/use_custom_user_dir", false);
	GLOBAL_DEF("application/config/custom_user_dir_name", "");
	GLOBAL_DEF("application/config/project_settings_override", "");

	GLOBAL_DEF("application/run/main_loop_type", "SceneTree");
	GLOBAL_DEF("application/config/auto_accept_quit", true);
	GLOBAL_DEF("application/config/quit_on_go_back", true);

	// The default window size is tuned to:
	// - Have a 16:9 aspect ratio,
	// - Have both dimensions divisible by 8 to better play along with video recording,
	// - Be displayable correctly in windowed mode on a 1366×768 display (tested on Windows 10 with default settings).
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "display/window/size/viewport_width", PROPERTY_HINT_RANGE, "1,7680,1,or_greater"), 1152); // 8K resolution
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "display/window/size/viewport_height", PROPERTY_HINT_RANGE, "1,4320,1,or_greater"), 648); // 8K resolution

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "display/window/size/mode", PROPERTY_HINT_ENUM, "Windowed,Minimized,Maximized,Fullscreen,Exclusive Fullscreen"), 0);

	// Keep the enum values in sync with the `DisplayServer::SCREEN_` enum.
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "display/window/size/initial_position_type", PROPERTY_HINT_ENUM, "Absolute,Center of Primary Screen,Center of Other Screen,Center of Screen With Mouse Pointer,Center of Screen With Keyboard Focus"), 1);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::VECTOR2I, "display/window/size/initial_position"), Vector2i());
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "display/window/size/initial_screen", PROPERTY_HINT_RANGE, "0,64,1,or_greater"), 0);

	GLOBAL_DEF_BASIC("display/window/size/resizable", true);
	GLOBAL_DEF_BASIC("display/window/size/borderless", false);
	GLOBAL_DEF("display/window/size/always_on_top", false);
	GLOBAL_DEF("display/window/size/transparent", false);
	GLOBAL_DEF("display/window/size/extend_to_title", false);
	GLOBAL_DEF("display/window/size/no_focus", false);
	GLOBAL_DEF("display/window/size/sharp_corners", false);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "display/window/size/window_width_override", PROPERTY_HINT_RANGE, "0,7680,1,or_greater"), 0); // 8K resolution
	GLOBAL_DEF(PropertyInfo(Variant::INT, "display/window/size/window_height_override", PROPERTY_HINT_RANGE, "0,4320,1,or_greater"), 0); // 8K resolution

	GLOBAL_DEF("display/window/energy_saving/keep_screen_on", true);
	GLOBAL_DEF("animation/warnings/check_invalid_track_paths", true);
	GLOBAL_DEF("animation/warnings/check_angle_interpolation_type_conflicting", true);

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "audio/buses/default_bus_layout", PROPERTY_HINT_FILE, "*.tres"), "res://default_bus_layout.tres");
	GLOBAL_DEF(PropertyInfo(Variant::INT, "audio/general/default_playback_type", PROPERTY_HINT_ENUM, "Stream,Sample"), 0);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "audio/general/default_playback_type.web", PROPERTY_HINT_ENUM, "Stream,Sample"), 1);
	GLOBAL_DEF_RST("audio/general/text_to_speech", false);
	GLOBAL_DEF_RST(PropertyInfo(Variant::FLOAT, "audio/general/2d_panning_strength", PROPERTY_HINT_RANGE, "0,2,0.01"), 0.5f);
	GLOBAL_DEF_RST(PropertyInfo(Variant::FLOAT, "audio/general/3d_panning_strength", PROPERTY_HINT_RANGE, "0,2,0.01"), 0.5f);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "audio/general/ios/session_category", PROPERTY_HINT_ENUM, "Ambient,Multi Route,Play and Record,Playback,Record,Solo Ambient"), 0);
	GLOBAL_DEF("audio/general/ios/mix_with_others", false);

	_add_builtin_input_map();

	// Keep the enum values in sync with the `DisplayServer::ScreenOrientation` enum.
	custom_prop_info["display/window/handheld/orientation"] = PropertyInfo(Variant::INT, "display/window/handheld/orientation", PROPERTY_HINT_ENUM, "Landscape,Portrait,Reverse Landscape,Reverse Portrait,Sensor Landscape,Sensor Portrait,Sensor");
	GLOBAL_DEF("display/window/subwindows/embed_subwindows", true);
	// Keep the enum values in sync with the `DisplayServer::VSyncMode` enum.
	custom_prop_info["display/window/vsync/vsync_mode"] = PropertyInfo(Variant::INT, "display/window/vsync/vsync_mode", PROPERTY_HINT_ENUM, "Disabled,Enabled,Adaptive,Mailbox");

	GLOBAL_DEF("display/window/frame_pacing/android/enable_frame_pacing", true);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "display/window/frame_pacing/android/swappy_mode", PROPERTY_HINT_ENUM, "pipeline_forced_on,auto_fps_pipeline_forced_on,auto_fps_auto_pipeline"), 2);

#ifdef DISABLE_DEPRECATED
	custom_prop_info["rendering/driver/threads/thread_model"] = PropertyInfo(Variant::INT, "rendering/driver/threads/thread_model", PROPERTY_HINT_ENUM, "Safe:1,Separate");
#else
	custom_prop_info["rendering/driver/threads/thread_model"] = PropertyInfo(Variant::INT, "rendering/driver/threads/thread_model", PROPERTY_HINT_ENUM, "Unsafe (deprecated),Safe,Separate");
#endif
	GLOBAL_DEF("physics/2d/run_on_separate_thread", false);
	GLOBAL_DEF("physics/3d/run_on_separate_thread", false);

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "display/window/stretch/mode", PROPERTY_HINT_ENUM, "disabled,canvas_items,viewport"), "disabled");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "display/window/stretch/aspect", PROPERTY_HINT_ENUM, "ignore,keep,keep_width,keep_height,expand"), "keep");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "display/window/stretch/scale", PROPERTY_HINT_RANGE, "0.5,8.0,0.01"), 1.0);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "display/window/stretch/scale_mode", PROPERTY_HINT_ENUM, "fractional,integer"), "fractional");

	GLOBAL_DEF(PropertyInfo(Variant::INT, "debug/settings/profiler/max_functions", PROPERTY_HINT_RANGE, "128,65535,1"), 16384);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "debug/settings/profiler/max_timestamp_query_elements", PROPERTY_HINT_RANGE, "256,65535,1"), 256);

	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "compression/formats/zstd/long_distance_matching"), Compression::zstd_long_distance_matching);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "compression/formats/zstd/compression_level", PROPERTY_HINT_RANGE, "1,22,1"), Compression::zstd_level);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "compression/formats/zstd/window_log_size", PROPERTY_HINT_RANGE, "10,30,1"), Compression::zstd_window_log_size);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "compression/formats/zlib/compression_level", PROPERTY_HINT_RANGE, "-1,9,1"), Compression::zlib_level);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "compression/formats/gzip/compression_level", PROPERTY_HINT_RANGE, "-1,9,1"), Compression::gzip_level);

	GLOBAL_DEF("debug/settings/crash_handler/message",
			String("Please include this when reporting the bug to the project developer."));
	GLOBAL_DEF("debug/settings/crash_handler/message.editor",
			String("Please include this when reporting the bug on: https://github.com/godotengine/godot/issues"));
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/occlusion_culling/bvh_build_quality", PROPERTY_HINT_ENUM, "Low,Medium,High"), 2);
	GLOBAL_DEF_RST("rendering/occlusion_culling/jitter_projection", true);

	GLOBAL_DEF_RST("internationalization/rendering/force_right_to_left_layout_direction", false);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "internationalization/rendering/root_node_layout_direction", PROPERTY_HINT_ENUM, "Based on Application Locale,Left-to-Right,Right-to-Left,Based on System Locale"), 0);
	GLOBAL_DEF_BASIC("internationalization/rendering/root_node_auto_translate", true);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "gui/timers/incremental_search_max_interval_msec", PROPERTY_HINT_RANGE, "0,10000,1,or_greater"), 2000);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "gui/timers/tooltip_delay_sec", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater"), 0.5);
#ifdef TOOLS_ENABLED
	GLOBAL_DEF("gui/timers/tooltip_delay_sec.editor_hint", 0.5);
#endif

	GLOBAL_DEF_BASIC("gui/common/snap_controls_to_pixels", true);
	GLOBAL_DEF_BASIC("gui/fonts/dynamic_fonts/use_oversampling", true);

	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/rendering_device/vsync/frame_queue_size", PROPERTY_HINT_RANGE, "2,3,1"), 2);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/rendering_device/vsync/swapchain_image_count", PROPERTY_HINT_RANGE, "2,4,1"), 3);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/rendering_device/staging_buffer/block_size_kb", PROPERTY_HINT_RANGE, "4,2048,1,or_greater"), 256);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/rendering_device/staging_buffer/max_size_mb", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), 128);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/rendering_device/staging_buffer/texture_upload_region_size_px", PROPERTY_HINT_RANGE, "1,256,1,or_greater"), 64);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/rendering_device/staging_buffer/texture_download_region_size_px", PROPERTY_HINT_RANGE, "1,256,1,or_greater"), 64);
	GLOBAL_DEF_RST(PropertyInfo(Variant::BOOL, "rendering/rendering_device/pipeline_cache/enable"), true);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/rendering_device/pipeline_cache/save_chunk_size_mb", PROPERTY_HINT_RANGE, "0.000001,64.0,0.001,or_greater"), 3.0);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/rendering_device/vulkan/max_descriptors_per_pool", PROPERTY_HINT_RANGE, "1,256,1,or_greater"), 64);

	GLOBAL_DEF_RST("rendering/rendering_device/d3d12/max_resource_descriptors_per_frame", 16384);
	custom_prop_info["rendering/rendering_device/d3d12/max_resource_descriptors_per_frame"] = PropertyInfo(Variant::INT, "rendering/rendering_device/d3d12/max_resource_descriptors_per_frame", PROPERTY_HINT_RANGE, "512,262144");
	GLOBAL_DEF_RST("rendering/rendering_device/d3d12/max_sampler_descriptors_per_frame", 1024);
	custom_prop_info["rendering/rendering_device/d3d12/max_sampler_descriptors_per_frame"] = PropertyInfo(Variant::INT, "rendering/rendering_device/d3d12/max_sampler_descriptors_per_frame", PROPERTY_HINT_RANGE, "256,2048");
	GLOBAL_DEF_RST("rendering/rendering_device/d3d12/max_misc_descriptors_per_frame", 512);
	custom_prop_info["rendering/rendering_device/d3d12/max_misc_descriptors_per_frame"] = PropertyInfo(Variant::INT, "rendering/rendering_device/d3d12/max_misc_descriptors_per_frame", PROPERTY_HINT_RANGE, "32,4096");

	// The default value must match the minor part of the Agility SDK version
	// installed by the scripts provided in the repository
	// (check `misc/scripts/install_d3d12_sdk_windows.py`).
	// For example, if the script installs 1.613.3, the default value must be 613.
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "rendering/rendering_device/d3d12/agility_sdk_version", PROPERTY_HINT_RANGE, "0,10000,1,or_greater,hide_slider"), 613);

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "rendering/textures/canvas_textures/default_texture_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Linear Mipmap,Nearest Mipmap"), 1);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "rendering/textures/canvas_textures/default_texture_repeat", PROPERTY_HINT_ENUM, "Disable,Enable,Mirror"), 0);

	GLOBAL_DEF("collada/use_ambient", false);

	// Input settings
	GLOBAL_DEF_BASIC("input_devices/pointing/android/enable_long_press_as_right_click", false);
	GLOBAL_DEF_BASIC("input_devices/pointing/android/enable_pan_and_scale_gestures", false);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "input_devices/pointing/android/rotary_input_scroll_axis", PROPERTY_HINT_ENUM, "Horizontal,Vertical"), 1);

	// These properties will not show up in the dialog. If you want to exclude whole groups, use add_hidden_prefix().
	GLOBAL_DEF_INTERNAL("application/config/features", PackedStringArray());
	GLOBAL_DEF_INTERNAL("internationalization/locale/translation_remaps", PackedStringArray());
	GLOBAL_DEF_INTERNAL("internationalization/locale/translations", PackedStringArray());
	GLOBAL_DEF_INTERNAL("internationalization/locale/translations_pot_files", PackedStringArray());
	GLOBAL_DEF_INTERNAL("internationalization/locale/translation_add_builtin_strings_to_pot", false);

	ProjectSettings::get_singleton()->add_hidden_prefix("input/");
}

ProjectSettings::ProjectSettings(const String &p_path) {
	if (load_custom(p_path) == OK) {
		resource_path = p_path.get_base_dir();
		project_loaded = true;
	}
}

ProjectSettings::~ProjectSettings() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
