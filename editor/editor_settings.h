/**************************************************************************/
/*  editor_settings.h                                                     */
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

#include "core/input/shortcut.h"
#include "core/io/config_file.h"
#include "core/io/resource.h"
#include "core/os/thread_safe.h"

class EditorPlugin;

class EditorSettings : public Resource {
	GDCLASS(EditorSettings, Resource);

	_THREAD_SAFE_CLASS_

public:
	struct Plugin {
		EditorPlugin *instance = nullptr;
		String path;
		String name;
		String author;
		String version;
		String description;
		bool installs = false;
		String script;
		Vector<String> install_files;
	};

	enum NetworkMode {
		NETWORK_OFFLINE,
		NETWORK_ONLINE,
	};

	enum InitialScreen {
		INITIAL_SCREEN_AUTO = -5, // Remembers last screen position.
		INITIAL_SCREEN_WITH_MOUSE_FOCUS = -4,
		INITIAL_SCREEN_WITH_KEYBOARD_FOCUS = -3,
		INITIAL_SCREEN_PRIMARY = -2,
	};

private:
	struct VariantContainer {
		int order = 0;
		Variant variant;
		Variant initial;
		bool basic = false;
		bool has_default_value = false;
		bool hide_from_editor = false;
		bool save = false;
		bool restart_if_changed = false;

		VariantContainer() {}

		VariantContainer(const Variant &p_variant, int p_order) :
				order(p_order),
				variant(p_variant) {
		}
	};

	static Ref<EditorSettings> singleton;

	HashSet<String> changed_settings;

	mutable Ref<ConfigFile> project_metadata;
	HashMap<String, PropertyInfo> hints;
	HashMap<String, VariantContainer> props;
	int last_order;

	Ref<Resource> clipboard;
	mutable HashMap<String, Ref<Shortcut>> shortcuts;
	HashMap<String, List<Ref<InputEvent>>> builtin_action_overrides;

	Vector<String> favorites;
	HashMap<String, PackedStringArray> favorite_properties;
	Vector<String> recent_dirs;

	bool save_changed_setting = true;
	bool optimize_save = true; //do not save stuff that came from config but was not set from engine
	bool initialized = false;

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _set_only(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _initial_set(const StringName &p_name, const Variant &p_value, bool p_basic = false);
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _add_property_info_bind(const Dictionary &p_info);
	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;

	void _set_initialized();
	void _load_defaults(Ref<ConfigFile> p_extra_config = Ref<ConfigFile>());
	void _load_godot2_text_editor_theme();
	void _load_default_visual_shader_editor_theme();
	bool _save_text_editor_theme(const String &p_file);
	bool _is_default_text_editor_theme(const String &p_theme_name);
	const String _get_project_metadata_path() const;
#ifndef DISABLE_DEPRECATED
	void _remove_deprecated_settings();
#endif

protected:
	static void _bind_methods();

public:
	enum {
		NOTIFICATION_EDITOR_SETTINGS_CHANGED = 10000
	};

	static EditorSettings *get_singleton();
	static String get_existing_settings_path();
	static String get_newest_settings_path();

	static void create();
	void setup_language();
	void setup_network();
	static void save();
	static void destroy();
	void set_optimize_save(bool p_optimize);

	bool has_default_value(const String &p_setting) const;
	void set_setting(const String &p_setting, const Variant &p_value);
	Variant get_setting(const String &p_setting) const;
	bool has_setting(const String &p_setting) const;
	void erase(const String &p_setting);
	void raise_order(const String &p_setting);
	void set_initial_value(const StringName &p_setting, const Variant &p_value, bool p_update_current = false);
	void set_restart_if_changed(const StringName &p_setting, bool p_restart);
	void set_basic(const StringName &p_setting, bool p_basic);
	void set_manually(const StringName &p_setting, const Variant &p_value, bool p_emit_signal = false) {
		if (p_emit_signal) {
			_set(p_setting, p_value);
		} else {
			_set_only(p_setting, p_value);
		}
	}
	void add_property_hint(const PropertyInfo &p_hint);
	PackedStringArray get_changed_settings() const;
	bool check_changed_settings_in_group(const String &p_setting_prefix) const;
	void mark_setting_changed(const String &p_setting);

	void set_resource_clipboard(const Ref<Resource> &p_resource) { clipboard = p_resource; }
	Ref<Resource> get_resource_clipboard() const { return clipboard; }

	void set_project_metadata(const String &p_section, const String &p_key, const Variant &p_data);
	Variant get_project_metadata(const String &p_section, const String &p_key, const Variant &p_default) const;

	void set_favorites(const Vector<String> &p_favorites);
	Vector<String> get_favorites() const;
	void set_favorite_properties(const HashMap<String, PackedStringArray> &p_favorite_properties);
	HashMap<String, PackedStringArray> get_favorite_properties() const;
	void set_recent_dirs(const Vector<String> &p_recent_dirs);
	Vector<String> get_recent_dirs() const;
	void load_favorites_and_recent_dirs();

	void list_text_editor_themes();
	void load_text_editor_theme();
	bool import_text_editor_theme(const String &p_file);
	bool save_text_editor_theme();
	bool save_text_editor_theme_as(String p_file);
	bool is_default_text_editor_theme();

	Vector<String> get_script_templates(const String &p_extension, const String &p_custom_path = String());
	String get_editor_layouts_config() const;
	float get_auto_display_scale() const;

	void _add_shortcut_default(const String &p_name, const Ref<Shortcut> &p_shortcut);
	void add_shortcut(const String &p_name, const Ref<Shortcut> &p_shortcut);
	bool is_shortcut(const String &p_name, const Ref<InputEvent> &p_event) const;
	Ref<Shortcut> get_shortcut(const String &p_name) const;
	void get_shortcut_list(List<String> *r_shortcuts);

	void set_builtin_action_override(const String &p_name, const TypedArray<InputEvent> &p_events);
	const Array get_builtin_action_overrides(const String &p_name) const;

	void notify_changes();

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	EditorSettings();
};

//not a macro any longer

#define EDITOR_DEF(m_var, m_val) _EDITOR_DEF(m_var, Variant(m_val))
#define EDITOR_DEF_RST(m_var, m_val) _EDITOR_DEF(m_var, Variant(m_val), true)
#define EDITOR_DEF_BASIC(m_var, m_val) _EDITOR_DEF(m_var, Variant(m_val), false, true)
Variant _EDITOR_DEF(const String &p_setting, const Variant &p_default, bool p_restart_if_changed = false, bool p_basic = false);

#define EDITOR_GET(m_var) _EDITOR_GET(m_var)
Variant _EDITOR_GET(const String &p_setting);

#define ED_IS_SHORTCUT(p_name, p_ev) (EditorSettings::get_singleton()->is_shortcut(p_name, p_ev))
Ref<Shortcut> ED_SHORTCUT(const String &p_path, const String &p_name, Key p_keycode = Key::NONE, bool p_physical = false);
Ref<Shortcut> ED_SHORTCUT_ARRAY(const String &p_path, const String &p_name, const PackedInt32Array &p_keycodes, bool p_physical = false);
void ED_SHORTCUT_OVERRIDE(const String &p_path, const String &p_feature, Key p_keycode = Key::NONE, bool p_physical = false);
void ED_SHORTCUT_OVERRIDE_ARRAY(const String &p_path, const String &p_feature, const PackedInt32Array &p_keycodes, bool p_physical = false);
Ref<Shortcut> ED_GET_SHORTCUT(const String &p_path);
