/**************************************************************************/
/*  project_settings.h                                                    */
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

#ifndef PROJECT_SETTINGS_H
#define PROJECT_SETTINGS_H

#include "core/object/class_db.h"

template <typename T>
class TypedArray;

class ProjectSettings : public Object {
	GDCLASS(ProjectSettings, Object);
	_THREAD_SAFE_CLASS_
	friend class TestProjectSettingsInternalsAccessor;

	bool is_changed = false;

public:
	typedef HashMap<String, Variant> CustomMap;
	static const String PROJECT_DATA_DIR_NAME_SUFFIX;

	// Properties that are not for built in values begin from this value, so builtin ones are displayed first.
	constexpr static const int32_t NO_BUILTIN_ORDER_BASE = 1 << 16;

#ifdef TOOLS_ENABLED
	const static PackedStringArray get_required_features();
	const static PackedStringArray get_unsupported_features(const PackedStringArray &p_project_features);
#endif // TOOLS_ENABLED

	struct AutoloadInfo {
		StringName name;
		String path;
		bool is_singleton = false;
	};

protected:
	struct VariantContainer {
		int order = 0;
		bool persist = false;
		bool basic = false;
		bool internal = false;
		Variant variant;
		Variant initial;
		bool hide_from_editor = false;
		bool restart_if_changed = false;
#ifdef DEBUG_METHODS_ENABLED
		bool ignore_value_in_docs = false;
#endif

		VariantContainer() {}

		VariantContainer(const Variant &p_variant, int p_order, bool p_persist = false) :
				order(p_order),
				persist(p_persist),
				variant(p_variant) {
		}
	};

	int last_order = NO_BUILTIN_ORDER_BASE;
	int last_builtin_order = 0;
	uint64_t last_save_time = 0;

	RBMap<StringName, VariantContainer> props; // NOTE: Key order is used e.g. in the save_custom method.
	String resource_path;
	HashMap<StringName, PropertyInfo> custom_prop_info;
	bool using_datapack = false;
	bool project_loaded = false;
	List<String> input_presets;

	HashSet<String> custom_features;
	HashMap<StringName, LocalVector<Pair<StringName, StringName>>> feature_overrides;

	LocalVector<String> hidden_prefixes;
	HashMap<StringName, AutoloadInfo> autoloads;
	HashMap<StringName, String> global_groups;
	HashMap<StringName, HashSet<StringName>> scene_groups_cache;

	Array global_class_list;
	bool is_global_class_list_loaded = false;

	String project_data_dir_name;

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;

	void _queue_changed();
	void _emit_changed();

	static ProjectSettings *singleton;

	Error _load_settings_text(const String &p_path);
	Error _load_settings_binary(const String &p_path);
	Error _load_settings_text_or_binary(const String &p_text_path, const String &p_bin_path);

	Error _save_settings_text(const String &p_file, const RBMap<String, List<String>> &props, const CustomMap &p_custom = CustomMap(), const String &p_custom_features = String());
	Error _save_settings_binary(const String &p_file, const RBMap<String, List<String>> &props, const CustomMap &p_custom = CustomMap(), const String &p_custom_features = String());

	Error _save_custom_bnd(const String &p_file);

#ifdef TOOLS_ENABLED
	const static PackedStringArray _get_supported_features();
	const static PackedStringArray _trim_to_supported_features(const PackedStringArray &p_project_features);
#endif // TOOLS_ENABLED

	void _convert_to_last_version(int p_from_version);

	bool _load_resource_pack(const String &p_pack, bool p_replace_files = true, int p_offset = 0);

	void _add_property_info_bind(const Dictionary &p_info);

	Error _setup(const String &p_path, const String &p_main_pack, bool p_upwards = false, bool p_ignore_override = false);

	void _add_builtin_input_map();

protected:
	static void _bind_methods();

public:
	static const int CONFIG_VERSION = 5;

	void set_setting(const String &p_setting, const Variant &p_value);
	Variant get_setting(const String &p_setting, const Variant &p_default_value = Variant()) const;
	TypedArray<Dictionary> get_global_class_list();
	void refresh_global_class_list();
	void store_global_class_list(const Array &p_classes);
	String get_global_class_list_path() const;

	bool has_setting(const String &p_var) const;
	String localize_path(const String &p_path) const;
	String globalize_path(const String &p_path) const;

	void set_initial_value(const String &p_name, const Variant &p_value);
	void set_as_basic(const String &p_name, bool p_basic);
	void set_as_internal(const String &p_name, bool p_internal);
	void set_restart_if_changed(const String &p_name, bool p_restart);
	void set_ignore_value_in_docs(const String &p_name, bool p_ignore);
	bool get_ignore_value_in_docs(const String &p_name) const;
	void add_hidden_prefix(const String &p_prefix);

	String get_project_data_dir_name() const;
	String get_project_data_path() const;
	String get_resource_path() const;
	String get_imported_files_path() const;

	static ProjectSettings *get_singleton();

	void clear(const String &p_name);
	int get_order(const String &p_name) const;
	void set_order(const String &p_name, int p_order);
	void set_builtin_order(const String &p_name);
	bool is_builtin_setting(const String &p_name) const;

	Error setup(const String &p_path, const String &p_main_pack, bool p_upwards = false, bool p_ignore_override = false);

	Error load_custom(const String &p_path);
	Error save_custom(const String &p_path = "", const CustomMap &p_custom = CustomMap(), const Vector<String> &p_custom_features = Vector<String>(), bool p_merge_with_current = true);
	Error save();
	void set_custom_property_info(const PropertyInfo &p_info);
	const HashMap<StringName, PropertyInfo> &get_custom_property_info() const;
	uint64_t get_last_saved_time() { return last_save_time; }

	List<String> get_input_presets() const { return input_presets; }

	Variant get_setting_with_override(const StringName &p_name) const;

	bool is_using_datapack() const;
	bool is_project_loaded() const;

	bool has_custom_feature(const String &p_feature) const;

	const HashMap<StringName, AutoloadInfo> &get_autoload_list() const;
	void add_autoload(const AutoloadInfo &p_autoload);
	void remove_autoload(const StringName &p_autoload);
	bool has_autoload(const StringName &p_autoload) const;
	AutoloadInfo get_autoload(const StringName &p_name) const;

	const HashMap<StringName, String> &get_global_groups_list() const;
	void add_global_group(const StringName &p_name, const String &p_description);
	void remove_global_group(const StringName &p_name);
	bool has_global_group(const StringName &p_name) const;

	const HashMap<StringName, HashSet<StringName>> &get_scene_groups_cache() const;
	void add_scene_groups_cache(const StringName &p_path, const HashSet<StringName> &p_cache);
	void remove_scene_groups_cache(const StringName &p_path);
	void save_scene_groups_cache();
	String get_scene_groups_cache_path() const;
	void load_scene_groups_cache();

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	ProjectSettings();
	ProjectSettings(const String &p_path);
	~ProjectSettings();
};

// Not a macro any longer.
Variant _GLOBAL_DEF(const String &p_var, const Variant &p_default, bool p_restart_if_changed = false, bool p_ignore_value_in_docs = false, bool p_basic = false, bool p_internal = false);
Variant _GLOBAL_DEF(const PropertyInfo &p_info, const Variant &p_default, bool p_restart_if_changed = false, bool p_ignore_value_in_docs = false, bool p_basic = false, bool p_internal = false);

#define GLOBAL_DEF(m_var, m_value) _GLOBAL_DEF(m_var, m_value)
#define GLOBAL_DEF_RST(m_var, m_value) _GLOBAL_DEF(m_var, m_value, true)
#define GLOBAL_DEF_NOVAL(m_var, m_value) _GLOBAL_DEF(m_var, m_value, false, true)
#define GLOBAL_DEF_RST_NOVAL(m_var, m_value) _GLOBAL_DEF(m_var, m_value, true, true)
#define GLOBAL_GET(m_var) ProjectSettings::get_singleton()->get_setting_with_override(m_var)

#define GLOBAL_DEF_BASIC(m_var, m_value) _GLOBAL_DEF(m_var, m_value, false, false, true)
#define GLOBAL_DEF_RST_BASIC(m_var, m_value) _GLOBAL_DEF(m_var, m_value, true, false, true)
#define GLOBAL_DEF_NOVAL_BASIC(m_var, m_value) _GLOBAL_DEF(m_var, m_value, false, true, true)
#define GLOBAL_DEF_RST_NOVAL_BASIC(m_var, m_value) _GLOBAL_DEF(m_var, m_value, true, true, true)

#define GLOBAL_DEF_INTERNAL(m_var, m_value) _GLOBAL_DEF(m_var, m_value, false, false, false, true)

#endif // PROJECT_SETTINGS_H
