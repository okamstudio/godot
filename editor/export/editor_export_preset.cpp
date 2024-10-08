/**************************************************************************/
/*  editor_export_preset.cpp                                              */
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

#include "editor_export.h"

#include "core/config/project_settings.h"

bool EditorExportPreset::_set(const StringName &p_name, const Variant &p_value) {
	values[p_name] = p_value;
	EditorExport::singleton->save_presets();
	if (update_visibility.has(p_name)) {
		if (update_visibility[p_name]) {
			update_value_overrides();
			notify_property_list_changed();
		}
		return true;
	}

	return false;
}

bool EditorExportPreset::_get(const StringName &p_name, Variant &r_ret) const {
	if (value_overrides.has(p_name)) {
		r_ret = value_overrides[p_name];
		return true;
	}

	if (values.has(p_name)) {
		r_ret = values[p_name];
		return true;
	}

	return false;
}

void EditorExportPreset::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_get_property_warning", "name"), &EditorExportPreset::_get_property_warning);

	ClassDB::bind_method(D_METHOD("has", "property"), &EditorExportPreset::has);

	ClassDB::bind_method(D_METHOD("get_files_to_export"), &EditorExportPreset::get_files_to_export);
	ClassDB::bind_method(D_METHOD("get_customized_files"), &EditorExportPreset::get_customized_files);
	ClassDB::bind_method(D_METHOD("get_customized_files_count"), &EditorExportPreset::get_customized_files_count);
	ClassDB::bind_method(D_METHOD("has_export_file", "path"), &EditorExportPreset::has_export_file);
	ClassDB::bind_method(D_METHOD("get_file_export_mode", "path", "default"), &EditorExportPreset::get_file_export_mode, DEFVAL(MODE_FILE_NOT_CUSTOMIZED));

	ClassDB::bind_method(D_METHOD("get_preset_name"), &EditorExportPreset::get_name);
	ClassDB::bind_method(D_METHOD("is_runnable"), &EditorExportPreset::is_runnable);
	ClassDB::bind_method(D_METHOD("are_advanced_options_enabled"), &EditorExportPreset::are_advanced_options_enabled);
	ClassDB::bind_method(D_METHOD("is_dedicated_server"), &EditorExportPreset::is_dedicated_server);
	ClassDB::bind_method(D_METHOD("get_export_filter"), &EditorExportPreset::get_export_filter);
	ClassDB::bind_method(D_METHOD("get_include_filter"), &EditorExportPreset::get_include_filter);
	ClassDB::bind_method(D_METHOD("get_exclude_filter"), &EditorExportPreset::get_exclude_filter);
	ClassDB::bind_method(D_METHOD("get_custom_features"), &EditorExportPreset::get_custom_features);
	ClassDB::bind_method(D_METHOD("get_patches"), &EditorExportPreset::get_patches);
	ClassDB::bind_method(D_METHOD("get_export_path"), &EditorExportPreset::get_export_path);
	ClassDB::bind_method(D_METHOD("get_encryption_in_filter"), &EditorExportPreset::get_enc_in_filter);
	ClassDB::bind_method(D_METHOD("get_encryption_ex_filter"), &EditorExportPreset::get_enc_ex_filter);
	ClassDB::bind_method(D_METHOD("get_encrypt_pck"), &EditorExportPreset::get_enc_pck);
	ClassDB::bind_method(D_METHOD("get_encrypt_directory"), &EditorExportPreset::get_enc_directory);
	ClassDB::bind_method(D_METHOD("get_encryption_key"), &EditorExportPreset::get_script_encryption_key);
	ClassDB::bind_method(D_METHOD("get_script_export_mode"), &EditorExportPreset::get_script_export_mode);

	ClassDB::bind_method(D_METHOD("get_or_env", "name", "env_var"), &EditorExportPreset::_get_or_env);
	ClassDB::bind_method(D_METHOD("get_version", "name", "windows_version"), &EditorExportPreset::get_version);

	BIND_ENUM_CONSTANT(EXPORT_ALL_RESOURCES);
	BIND_ENUM_CONSTANT(EXPORT_SELECTED_SCENES);
	BIND_ENUM_CONSTANT(EXPORT_SELECTED_RESOURCES);
	BIND_ENUM_CONSTANT(EXCLUDE_SELECTED_RESOURCES);
	BIND_ENUM_CONSTANT(EXPORT_CUSTOMIZED);

	BIND_ENUM_CONSTANT(MODE_FILE_NOT_CUSTOMIZED);
	BIND_ENUM_CONSTANT(MODE_FILE_STRIP);
	BIND_ENUM_CONSTANT(MODE_FILE_KEEP);
	BIND_ENUM_CONSTANT(MODE_FILE_REMOVE);

	BIND_ENUM_CONSTANT(MODE_SCRIPT_TEXT);
	BIND_ENUM_CONSTANT(MODE_SCRIPT_BINARY_TOKENS);
	BIND_ENUM_CONSTANT(MODE_SCRIPT_BINARY_TOKENS_COMPRESSED);
}

String EditorExportPreset::_get_property_warning(const StringName &p_name) const {
	if (value_overrides.has(p_name)) {
		return String();
	}

	String warning = platform->get_export_option_warning(this, p_name);
	if (!warning.is_empty()) {
		warning += "\n";
	}

	// Get property warning from editor export plugins.
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	for (int i = 0; i < export_plugins.size(); i++) {
		if (!export_plugins[i]->supports_platform(platform)) {
			continue;
		}

		export_plugins.write[i]->set_export_preset(Ref<EditorExportPreset>(this));
		String plugin_warning = export_plugins[i]->_get_export_option_warning(platform, p_name);
		if (!plugin_warning.is_empty()) {
			warning += plugin_warning + "\n";
		}
	}

	return warning;
}

void EditorExportPreset::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const KeyValue<StringName, PropertyInfo> &E : properties) {
		if (!value_overrides.has(E.key)) {
			bool property_visible = platform->get_export_option_visibility(this, E.key);
			if (!property_visible) {
				continue;
			}

			// Get option visibility from editor export plugins.
			Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
			for (int i = 0; i < export_plugins.size(); i++) {
				if (!export_plugins[i]->supports_platform(platform)) {
					continue;
				}

				export_plugins.write[i]->set_export_preset(Ref<EditorExportPreset>(this));
				property_visible = export_plugins[i]->_get_export_option_visibility(platform, E.key);
				if (!property_visible) {
					break;
				}
			}

			if (property_visible) {
				p_list->push_back(E.value);
			}
		}
	}
}

Ref<EditorExportPlatform> EditorExportPreset::get_platform() const {
	return platform;
}

void EditorExportPreset::update_files() {
	{
		Vector<String> to_remove;
		for (const String &E : selected_files) {
			if (!FileAccess::exists(E)) {
				to_remove.push_back(E);
			}
		}
		for (int i = 0; i < to_remove.size(); ++i) {
			selected_files.erase(to_remove[i]);
		}
	}

	{
		Vector<String> to_remove;
		for (const KeyValue<String, FileExportMode> &E : customized_files) {
			if (!FileAccess::exists(E.key) && !DirAccess::exists(E.key)) {
				to_remove.push_back(E.key);
			}
		}
		for (int i = 0; i < to_remove.size(); ++i) {
			customized_files.erase(to_remove[i]);
		}
	}
}

void EditorExportPreset::update_value_overrides() {
	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	HashMap<StringName, Variant> new_value_overrides;

	value_overrides.clear();

	for (int i = 0; i < export_plugins.size(); i++) {
		if (!export_plugins[i]->supports_platform(platform)) {
			continue;
		}

		export_plugins.write[i]->set_export_preset(Ref<EditorExportPreset>(this));

		Dictionary plugin_overrides = export_plugins[i]->_get_export_options_overrides(platform);
		if (!plugin_overrides.is_empty()) {
			Array keys = plugin_overrides.keys();
			for (int x = 0; x < keys.size(); x++) {
				StringName key = keys[x];
				Variant value = plugin_overrides[key];
				if (new_value_overrides.has(key) && new_value_overrides[key] != value) {
					WARN_PRINT_ED(vformat("Editor export plugin '%s' overrides pre-existing export option override '%s' with new value.", export_plugins[i]->get_name(), key));
				}
				new_value_overrides[key] = value;
			}
		}
	}

	value_overrides = new_value_overrides;
	notify_property_list_changed();
}

Vector<String> EditorExportPreset::get_files_to_export() const {
	Vector<String> files;
	for (const String &E : selected_files) {
		files.push_back(E);
	}
	return files;
}

Dictionary EditorExportPreset::get_customized_files() const {
	Dictionary files;
	for (const KeyValue<String, FileExportMode> &E : customized_files) {
		String mode;
		switch (E.value) {
			case MODE_FILE_NOT_CUSTOMIZED: {
				continue;
			} break;
			case MODE_FILE_STRIP: {
				mode = "strip";
			} break;
			case MODE_FILE_KEEP: {
				mode = "keep";
			} break;
			case MODE_FILE_REMOVE: {
				mode = "remove";
			}
		}
		files[E.key] = mode;
	}
	return files;
}

int EditorExportPreset::get_customized_files_count() const {
	return customized_files.size();
}

void EditorExportPreset::set_customized_files(const Dictionary &p_files) {
	for (const Variant *key = p_files.next(nullptr); key; key = p_files.next(key)) {
		EditorExportPreset::FileExportMode mode = EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED;
		String value = p_files[*key];
		if (value == "strip") {
			mode = EditorExportPreset::MODE_FILE_STRIP;
		} else if (value == "keep") {
			mode = EditorExportPreset::MODE_FILE_KEEP;
		} else if (value == "remove") {
			mode = EditorExportPreset::MODE_FILE_REMOVE;
		}
		set_file_export_mode(*key, mode);
	}
}

void EditorExportPreset::set_name(const String &p_name) {
	name = p_name;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_name() const {
	return name;
}

void EditorExportPreset::set_runnable(bool p_enable) {
	runnable = p_enable;
	EditorExport::singleton->emit_presets_runnable_changed();
	EditorExport::singleton->save_presets();
}

bool EditorExportPreset::is_runnable() const {
	return runnable;
}

void EditorExportPreset::set_advanced_options_enabled(bool p_enabled) {
	if (advanced_options_enabled == p_enabled) {
		return;
	}
	advanced_options_enabled = p_enabled;
	EditorExport::singleton->save_presets();
	notify_property_list_changed();
}

bool EditorExportPreset::are_advanced_options_enabled() const {
	return advanced_options_enabled;
}

void EditorExportPreset::set_dedicated_server(bool p_enable) {
	dedicated_server = p_enable;
	EditorExport::singleton->save_presets();
}

bool EditorExportPreset::is_dedicated_server() const {
	return dedicated_server;
}

void EditorExportPreset::set_export_filter(ExportFilter p_filter) {
	export_filter = p_filter;
	EditorExport::singleton->save_presets();
}

EditorExportPreset::ExportFilter EditorExportPreset::get_export_filter() const {
	return export_filter;
}

void EditorExportPreset::set_include_filter(const String &p_include) {
	include_filter = p_include;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_include_filter() const {
	return include_filter;
}

void EditorExportPreset::set_export_path(const String &p_path) {
	export_path = p_path;
	/* NOTE(SonerSound): if there is a need to implement a PropertyHint that specifically indicates a relative path,
	 * this should be removed. */
	if (export_path.is_absolute_path()) {
		String res_path = OS::get_singleton()->get_resource_dir();
		export_path = res_path.path_to_file(export_path);
	}
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_export_path() const {
	return export_path;
}

void EditorExportPreset::set_exclude_filter(const String &p_exclude) {
	exclude_filter = p_exclude;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_exclude_filter() const {
	return exclude_filter;
}

void EditorExportPreset::add_export_file(const String &p_path) {
	selected_files.insert(p_path);
	EditorExport::singleton->save_presets();
}

void EditorExportPreset::remove_export_file(const String &p_path) {
	selected_files.erase(p_path);
	EditorExport::singleton->save_presets();
}

bool EditorExportPreset::has_export_file(const String &p_path) {
	return selected_files.has(p_path);
}

void EditorExportPreset::set_file_export_mode(const String &p_path, EditorExportPreset::FileExportMode p_mode) {
	if (p_mode == FileExportMode::MODE_FILE_NOT_CUSTOMIZED) {
		customized_files.erase(p_path);
	} else {
		customized_files.insert(p_path, p_mode);
	}
	EditorExport::singleton->save_presets();
}

EditorExportPreset::FileExportMode EditorExportPreset::get_file_export_mode(const String &p_path, EditorExportPreset::FileExportMode p_default) const {
	HashMap<String, FileExportMode>::ConstIterator i = customized_files.find(p_path);
	if (i) {
		return i->value;
	}
	return p_default;
}

void EditorExportPreset::add_patch(const String &p_path, int p_at_pos) {
	ERR_FAIL_COND_EDMSG(patches.has(p_path), vformat("Failed to add patch \"%s\". Patches must be unique.", p_path));

	if (p_at_pos < 0) {
		patches.push_back(p_path);
	} else {
		patches.insert(p_at_pos, p_path);
	}

	EditorExport::singleton->save_presets();
}

void EditorExportPreset::set_patch(int p_index, const String &p_path) {
	remove_patch(p_index);
	add_patch(p_path, p_index);
}

String EditorExportPreset::get_patch(int p_index) {
	ERR_FAIL_INDEX_V(p_index, patches.size(), String());
	return patches[p_index];
}

void EditorExportPreset::remove_patch(int p_index) {
	ERR_FAIL_INDEX(p_index, patches.size());
	patches.remove_at(p_index);
	EditorExport::singleton->save_presets();
}

void EditorExportPreset::set_patches(const Vector<String> &p_patches) {
	patches = p_patches;
}

Vector<String> EditorExportPreset::get_patches() const {
	return patches;
}

void EditorExportPreset::set_custom_features(const String &p_custom_features) {
	custom_features = p_custom_features;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_custom_features() const {
	return custom_features;
}

void EditorExportPreset::set_enc_in_filter(const String &p_filter) {
	enc_in_filters = p_filter;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_enc_in_filter() const {
	return enc_in_filters;
}

void EditorExportPreset::set_enc_ex_filter(const String &p_filter) {
	enc_ex_filters = p_filter;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_enc_ex_filter() const {
	return enc_ex_filters;
}

void EditorExportPreset::set_enc_pck(bool p_enabled) {
	enc_pck = p_enabled;
	EditorExport::singleton->save_presets();
}

bool EditorExportPreset::get_enc_pck() const {
	return enc_pck;
}

void EditorExportPreset::set_enc_directory(bool p_enabled) {
	enc_directory = p_enabled;
	EditorExport::singleton->save_presets();
}

bool EditorExportPreset::get_enc_directory() const {
	return enc_directory;
}

void EditorExportPreset::set_script_encryption_key(const String &p_key) {
	script_key = p_key;
	EditorExport::singleton->save_presets();
}

String EditorExportPreset::get_script_encryption_key() const {
	return script_key;
}

void EditorExportPreset::set_script_export_mode(int p_mode) {
	script_mode = p_mode;
	EditorExport::singleton->save_presets();
}

int EditorExportPreset::get_script_export_mode() const {
	return script_mode;
}

Variant EditorExportPreset::get_or_env(const StringName &p_name, const String &p_env_var, bool *r_valid) const {
	const String from_env = OS::get_singleton()->get_environment(p_env_var);
	if (!from_env.is_empty()) {
		if (r_valid) {
			*r_valid = true;
		}
		return from_env;
	}
	return get(p_name, r_valid);
}

_FORCE_INLINE_ bool _check_digits(const String &p_str) {
	for (int i = 0; i < p_str.length(); i++) {
		char32_t c = p_str.operator[](i);
		if (!is_digit(c)) {
			return false;
		}
	}
	return true;
}

String EditorExportPreset::get_version(const StringName &p_preset_string, bool p_windows_version) const {
	String result = get(p_preset_string);
	if (result.is_empty()) {
		result = GLOBAL_GET("application/config/version");

		// Split and validate version number components.
		const PackedStringArray result_split = result.split(".", false);
		bool valid_version = !result_split.is_empty();
		for (const String &E : result_split) {
			if (!_check_digits(E)) {
				valid_version = false;
				break;
			}
		}

		if (valid_version) {
			if (p_windows_version) {
				// Modify version number to match Windows constraints (version numbers must have 4 components).
				if (result_split.size() == 1) {
					result = result + ".0.0.0";
				} else if (result_split.size() == 2) {
					result = result + ".0.0";
				} else if (result_split.size() == 3) {
					result = result + ".0";
				} else {
					result = vformat("%s.%s.%s.%s", result_split[0], result_split[1], result_split[2], result_split[3]);
				}
			} else {
				result = String(".").join(result_split);
			}
		} else {
			if (!result.is_empty()) {
				WARN_PRINT(vformat("Invalid version number \"%s\". The version number can only contain numeric characters (0-9) and non-consecutive periods (.).", result));
			}
			if (p_windows_version) {
				result = "1.0.0.0";
			} else {
				result = "1.0.0";
			}
		}
	}

	return result;
}

EditorExportPreset::EditorExportPreset() {}
