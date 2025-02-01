/**************************************************************************/
/*  editor_export_preset.h                                                */
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

class EditorExportPlatform;

#include "core/object/ref_counted.h"

class EditorExportPreset : public RefCounted {
	GDCLASS(EditorExportPreset, RefCounted);

public:
	enum ExportFilter {
		EXPORT_ALL_RESOURCES,
		EXPORT_SELECTED_SCENES,
		EXPORT_SELECTED_RESOURCES,
		EXCLUDE_SELECTED_RESOURCES,
		EXPORT_CUSTOMIZED,
	};

	enum FileExportMode {
		MODE_FILE_NOT_CUSTOMIZED,
		MODE_FILE_STRIP,
		MODE_FILE_KEEP,
		MODE_FILE_REMOVE,
	};

	enum ScriptExportMode {
		MODE_SCRIPT_TEXT,
		MODE_SCRIPT_BINARY_TOKENS,
		MODE_SCRIPT_BINARY_TOKENS_COMPRESSED,
	};

private:
	Ref<EditorExportPlatform> platform;
	ExportFilter export_filter = EXPORT_ALL_RESOURCES;
	String include_filter;
	String exclude_filter;
	String export_path;

	String exporter;
	HashSet<String> selected_files;
	HashMap<String, FileExportMode> customized_files;
	bool runnable = false;
	bool advanced_options_enabled = false;
	bool dedicated_server = false;

	Vector<String> patches;

	friend class EditorExport;
	friend class EditorExportPlatform;

	HashMap<StringName, PropertyInfo> properties;
	HashMap<StringName, Variant> values;
	HashMap<StringName, Variant> value_overrides;
	HashMap<StringName, bool> update_visibility;

	String name;

	String custom_features;

	String enc_in_filters;
	String enc_ex_filters;
	bool enc_pck = false;
	bool enc_directory = false;
	uint64_t seed = 0;

	String script_key;
	int script_mode = MODE_SCRIPT_BINARY_TOKENS_COMPRESSED;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	String _get_property_warning(const StringName &p_name) const;

	static void _bind_methods();

public:
	Ref<EditorExportPlatform> get_platform() const;

	bool has(const StringName &p_property) const { return values.has(p_property); }

	void update_files();
	void update_value_overrides();

	Vector<String> get_files_to_export() const;
	Dictionary get_customized_files() const;
	int get_customized_files_count() const;
	void set_customized_files(const Dictionary &p_files);

	void add_export_file(const String &p_path);
	void remove_export_file(const String &p_path);
	bool has_export_file(const String &p_path);

	void set_file_export_mode(const String &p_path, FileExportMode p_mode);
	FileExportMode get_file_export_mode(const String &p_path, FileExportMode p_default = MODE_FILE_NOT_CUSTOMIZED) const;

	void set_name(const String &p_name);
	String get_name() const;

	void set_runnable(bool p_enable);
	bool is_runnable() const;

	void set_advanced_options_enabled(bool p_enabled);
	bool are_advanced_options_enabled() const;

	void set_dedicated_server(bool p_enable);
	bool is_dedicated_server() const;

	void set_export_filter(ExportFilter p_filter);
	ExportFilter get_export_filter() const;

	void set_include_filter(const String &p_include);
	String get_include_filter() const;

	void set_exclude_filter(const String &p_exclude);
	String get_exclude_filter() const;

	void add_patch(const String &p_path, int p_at_pos = -1);
	void set_patch(int p_index, const String &p_path);
	String get_patch(int p_index);
	void remove_patch(int p_index);
	void set_patches(const Vector<String> &p_patches);
	Vector<String> get_patches() const;

	void set_custom_features(const String &p_custom_features);
	String get_custom_features() const;

	void set_export_path(const String &p_path);
	String get_export_path() const;

	void set_enc_in_filter(const String &p_filter);
	String get_enc_in_filter() const;

	void set_enc_ex_filter(const String &p_filter);
	String get_enc_ex_filter() const;

	void set_seed(uint64_t p_seed);
	uint64_t get_seed() const;

	void set_enc_pck(bool p_enabled);
	bool get_enc_pck() const;

	void set_enc_directory(bool p_enabled);
	bool get_enc_directory() const;

	void set_script_encryption_key(const String &p_key);
	String get_script_encryption_key() const;

	void set_script_export_mode(int p_mode);
	int get_script_export_mode() const;

	Variant _get_or_env(const StringName &p_name, const String &p_env_var) const {
		return get_or_env(p_name, p_env_var);
	}
	Variant get_or_env(const StringName &p_name, const String &p_env_var, bool *r_valid = nullptr) const;

	// Return the preset's version number, or fall back to the
	// `application/config/version` project setting if set to an empty string.
	// If `p_windows_version` is `true`, formats the returned version number to
	// be compatible with Windows executable metadata (which requires a
	// 4-component format).
	String get_version(const StringName &p_name, bool p_windows_version = false) const;

	const HashMap<StringName, PropertyInfo> &get_properties() const { return properties; }
	const HashMap<StringName, Variant> &get_values() const { return values; }

	EditorExportPreset();
};

VARIANT_ENUM_CAST(EditorExportPreset::ExportFilter);
VARIANT_ENUM_CAST(EditorExportPreset::FileExportMode);
VARIANT_ENUM_CAST(EditorExportPreset::ScriptExportMode);
