/**************************************************************************/
/*  editor_export.cpp                                                     */
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
#include "core/io/config_file.h"
#include "editor/editor_settings.h"

EditorExport *EditorExport::singleton = nullptr;

void EditorExport::_save() {
	Ref<ConfigFile> config;
	Ref<ConfigFile> credentials;
	config.instantiate();
	credentials.instantiate();
	for (int i = 0; i < export_presets.size(); i++) {
		Ref<EditorExportPreset> preset = export_presets[i];
		String section = "preset." + itos(i);

		config->set_value(section, "name", preset->get_name());
		config->set_value(section, "platform", preset->get_platform()->get_name());
		config->set_value(section, "runnable", preset->is_runnable());
		config->set_value(section, "advanced_options", preset->are_advanced_options_enabled());
		config->set_value(section, "dedicated_server", preset->is_dedicated_server());
		config->set_value(section, "custom_features", preset->get_custom_features());

		bool save_files = false;
		switch (preset->get_export_filter()) {
			case EditorExportPreset::EXPORT_ALL_RESOURCES: {
				config->set_value(section, "export_filter", "all_resources");
			} break;
			case EditorExportPreset::EXPORT_SELECTED_SCENES: {
				config->set_value(section, "export_filter", "scenes");
				save_files = true;
			} break;
			case EditorExportPreset::EXPORT_SELECTED_RESOURCES: {
				config->set_value(section, "export_filter", "resources");
				save_files = true;
			} break;
			case EditorExportPreset::EXCLUDE_SELECTED_RESOURCES: {
				config->set_value(section, "export_filter", "exclude");
				save_files = true;
			} break;
			case EditorExportPreset::EXPORT_CUSTOMIZED: {
				config->set_value(section, "export_filter", "customized");
				config->set_value(section, "customized_files", preset->get_customized_files());
				save_files = false;
			};
		}

		if (save_files) {
			Vector<String> export_files = preset->get_files_to_export();
			config->set_value(section, "export_files", export_files);
		}
		config->set_value(section, "include_filter", preset->get_include_filter());
		config->set_value(section, "exclude_filter", preset->get_exclude_filter());
		config->set_value(section, "export_path", preset->get_export_path());
		config->set_value(section, "patches", preset->get_patches());

		config->set_value(section, "encryption_include_filters", preset->get_enc_in_filter());
		config->set_value(section, "encryption_exclude_filters", preset->get_enc_ex_filter());
		config->set_value(section, "seed", preset->get_seed());

		config->set_value(section, "encrypt_pck", preset->get_enc_pck());
		config->set_value(section, "encrypt_directory", preset->get_enc_directory());
		config->set_value(section, "script_export_mode", preset->get_script_export_mode());
		credentials->set_value(section, "script_encryption_key", preset->get_script_encryption_key());

		String option_section = "preset." + itos(i) + ".options";

		for (const KeyValue<StringName, Variant> &E : preset->values) {
			PropertyInfo *prop = preset->properties.getptr(E.key);
			if (prop && prop->usage & PROPERTY_USAGE_SECRET) {
				credentials->set_value(option_section, E.key, E.value);
			} else {
				config->set_value(option_section, E.key, E.value);
			}
		}
	}

	config->save("res://export_presets.cfg");
	credentials->save("res://.godot/export_credentials.cfg");
}

void EditorExport::save_presets() {
	if (block_save) {
		return;
	}
	save_timer->start();
}

void EditorExport::emit_presets_runnable_changed() {
	emit_signal(_export_presets_runnable_updated);
}

void EditorExport::_bind_methods() {
	ADD_SIGNAL(MethodInfo(_export_presets_updated));
	ADD_SIGNAL(MethodInfo(_export_presets_runnable_updated));
}

void EditorExport::add_export_platform(const Ref<EditorExportPlatform> &p_platform) {
	export_platforms.push_back(p_platform);

	should_update_presets = true;
	should_reload_presets = true;
}

void EditorExport::remove_export_platform(const Ref<EditorExportPlatform> &p_platform) {
	export_platforms.erase(p_platform);
	p_platform->cleanup();

	should_update_presets = true;
	should_reload_presets = true;
}

int EditorExport::get_export_platform_count() {
	return export_platforms.size();
}

Ref<EditorExportPlatform> EditorExport::get_export_platform(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, export_platforms.size(), Ref<EditorExportPlatform>());

	return export_platforms[p_idx];
}

void EditorExport::add_export_preset(const Ref<EditorExportPreset> &p_preset, int p_at_pos) {
	if (p_at_pos < 0) {
		export_presets.push_back(p_preset);
	} else {
		export_presets.insert(p_at_pos, p_preset);
	}
	emit_presets_runnable_changed();
}

int EditorExport::get_export_preset_count() const {
	return export_presets.size();
}

Ref<EditorExportPreset> EditorExport::get_export_preset(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, export_presets.size(), Ref<EditorExportPreset>());
	return export_presets[p_idx];
}

void EditorExport::remove_export_preset(int p_idx) {
	export_presets.remove_at(p_idx);
	save_presets();
	emit_presets_runnable_changed();
}

void EditorExport::add_export_plugin(const Ref<EditorExportPlugin> &p_plugin) {
	if (!export_plugins.has(p_plugin)) {
		export_plugins.push_back(p_plugin);
		should_update_presets = true;
	}
}

void EditorExport::remove_export_plugin(const Ref<EditorExportPlugin> &p_plugin) {
	export_plugins.erase(p_plugin);
	should_update_presets = true;
}

Vector<Ref<EditorExportPlugin>> EditorExport::get_export_plugins() {
	return export_plugins;
}

void EditorExport::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			load_config();
		} break;

		case NOTIFICATION_PROCESS: {
			update_export_presets();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			for (int i = 0; i < export_platforms.size(); i++) {
				export_platforms.write[i]->cleanup();
			}
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			for (int i = 0; i < export_platforms.size(); i++) {
				export_platforms.write[i]->notification(p_what);
			}
		} break;
	}
}

void EditorExport::load_config() {
	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load("res://export_presets.cfg");
	if (err != OK) {
		return;
	}

	Ref<ConfigFile> credentials;
	credentials.instantiate();
	err = credentials->load("res://.godot/export_credentials.cfg");
	if (!(err == OK || err == ERR_FILE_NOT_FOUND)) {
		return;
	}

	block_save = true;

	int index = 0;
	while (true) {
		String section = "preset." + itos(index);
		if (!config->has_section(section)) {
			break;
		}

		String platform = config->get_value(section, "platform");
#ifndef DISABLE_DEPRECATED
		// Compatibility with Linux platform before 4.3.
		if (platform == "Linux/X11") {
			platform = "Linux";
		}
#endif

		Ref<EditorExportPreset> preset;

		for (int i = 0; i < export_platforms.size(); i++) {
			if (export_platforms[i]->get_name() == platform) {
				preset = export_platforms.write[i]->create_preset();
				break;
			}
		}

		if (preset.is_null()) {
			index++;
			continue; // Unknown platform, skip without error (platform might be loaded later).
		}

		preset->set_name(config->get_value(section, "name"));
		preset->set_advanced_options_enabled(config->get_value(section, "advanced_options", false));
		preset->set_runnable(config->get_value(section, "runnable"));
		preset->set_dedicated_server(config->get_value(section, "dedicated_server", false));

		if (config->has_section_key(section, "custom_features")) {
			preset->set_custom_features(config->get_value(section, "custom_features"));
		}

		String export_filter = config->get_value(section, "export_filter");

		bool get_files = false;

		if (export_filter == "all_resources") {
			preset->set_export_filter(EditorExportPreset::EXPORT_ALL_RESOURCES);
		} else if (export_filter == "scenes") {
			preset->set_export_filter(EditorExportPreset::EXPORT_SELECTED_SCENES);
			get_files = true;
		} else if (export_filter == "resources") {
			preset->set_export_filter(EditorExportPreset::EXPORT_SELECTED_RESOURCES);
			get_files = true;
		} else if (export_filter == "exclude") {
			preset->set_export_filter(EditorExportPreset::EXCLUDE_SELECTED_RESOURCES);
			get_files = true;
		} else if (export_filter == "customized") {
			preset->set_export_filter(EditorExportPreset::EXPORT_CUSTOMIZED);
			preset->set_customized_files(config->get_value(section, "customized_files", Dictionary()));
			get_files = false;
		}

		if (get_files) {
			Vector<String> files = config->get_value(section, "export_files");

			for (int i = 0; i < files.size(); i++) {
				if (!FileAccess::exists(files[i])) {
					preset->remove_export_file(files[i]);
				} else {
					preset->add_export_file(files[i]);
				}
			}
		}

		preset->set_include_filter(config->get_value(section, "include_filter"));
		preset->set_exclude_filter(config->get_value(section, "exclude_filter"));
		preset->set_export_path(config->get_value(section, "export_path", ""));
		preset->set_script_export_mode(config->get_value(section, "script_export_mode", EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED));
		preset->set_patches(config->get_value(section, "patches", Vector<String>()));

		if (config->has_section_key(section, "seed")) {
			preset->set_seed(config->get_value(section, "seed"));
		}
		if (config->has_section_key(section, "encrypt_pck")) {
			preset->set_enc_pck(config->get_value(section, "encrypt_pck"));
		}
		if (config->has_section_key(section, "encrypt_directory")) {
			preset->set_enc_directory(config->get_value(section, "encrypt_directory"));
		}
		if (config->has_section_key(section, "encryption_include_filters")) {
			preset->set_enc_in_filter(config->get_value(section, "encryption_include_filters"));
		}
		if (config->has_section_key(section, "encryption_exclude_filters")) {
			preset->set_enc_ex_filter(config->get_value(section, "encryption_exclude_filters"));
		}
		if (credentials->has_section_key(section, "script_encryption_key")) {
			preset->set_script_encryption_key(credentials->get_value(section, "script_encryption_key"));
		}

		String option_section = "preset." + itos(index) + ".options";

		List<String> options;
		config->get_section_keys(option_section, &options);

		for (const String &E : options) {
			Variant value = config->get_value(option_section, E);
			preset->set(E, value);
		}

		if (credentials->has_section(option_section)) {
			options.clear();
			credentials->get_section_keys(option_section, &options);

			for (const String &E : options) {
				// Drop values for secret properties that no longer exist, or during the next save they would end up in the regular config file.
				if (preset->get_properties().has(E)) {
					Variant value = credentials->get_value(option_section, E);
					preset->set(E, value);
				}
			}
		}

		add_export_preset(preset);
		index++;
	}

	block_save = false;
}

void EditorExport::update_export_presets() {
	HashMap<StringName, List<EditorExportPlatform::ExportOption>> platform_options;

	if (should_reload_presets) {
		should_reload_presets = false;
		export_presets.clear();
		load_config();
	}

	for (int i = 0; i < export_platforms.size(); i++) {
		Ref<EditorExportPlatform> platform = export_platforms[i];

		bool should_update = should_update_presets;
		should_update |= platform->should_update_export_options();
		for (int j = 0; j < export_plugins.size(); j++) {
			should_update |= export_plugins.write[j]->_should_update_export_options(platform);
		}

		if (should_update) {
			List<EditorExportPlatform::ExportOption> options;
			platform->get_export_options(&options);

			for (int j = 0; j < export_plugins.size(); j++) {
				export_plugins[j]->_get_export_options(platform, &options);
			}

			platform_options[platform->get_name()] = options;
		}
	}
	should_update_presets = false;

	bool export_presets_updated = false;
	for (int i = 0; i < export_presets.size(); i++) {
		Ref<EditorExportPreset> preset = export_presets[i];
		if (platform_options.has(preset->get_platform()->get_name())) {
			export_presets_updated = true;

			bool update_value_overrides = false;
			List<EditorExportPlatform::ExportOption> options = platform_options[preset->get_platform()->get_name()];

			// Clear the preset properties prior to reloading, keep the values to preserve options from plugins that may be currently disabled.
			preset->properties.clear();
			preset->update_visibility.clear();

			for (const EditorExportPlatform::ExportOption &E : options) {
				StringName option_name = E.option.name;
				preset->properties[option_name] = E.option;
				if (!preset->has(option_name)) {
					preset->values[option_name] = E.default_value;
				}
				preset->update_visibility[option_name] = E.update_visibility;
				if (E.update_visibility) {
					update_value_overrides = true;
				}
			}

			if (update_value_overrides) {
				preset->update_value_overrides();
			}
		}
	}

	if (export_presets_updated) {
		emit_signal(_export_presets_updated);
	}
}

bool EditorExport::poll_export_platforms() {
	bool changed = false;
	for (int i = 0; i < export_platforms.size(); i++) {
		if (export_platforms.write[i]->poll_export()) {
			changed = true;
		}
	}

	return changed;
}

void EditorExport::connect_presets_runnable_updated(const Callable &p_target) {
	connect(_export_presets_runnable_updated, p_target);
}

EditorExport::EditorExport() {
	save_timer = memnew(Timer);
	add_child(save_timer);
	save_timer->set_wait_time(0.8);
	save_timer->set_one_shot(true);
	save_timer->connect("timeout", callable_mp(this, &EditorExport::_save));

	_export_presets_updated = StringName("export_presets_updated", true);
	_export_presets_runnable_updated = StringName("export_presets_runnable_updated", true);

	singleton = this;
	set_process(true);
}

EditorExport::~EditorExport() {
}
