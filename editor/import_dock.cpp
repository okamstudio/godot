/**************************************************************************/
/*  import_dock.cpp                                                       */
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

#include "import_dock.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"

class ImportDockParameters : public Object {
	GDCLASS(ImportDockParameters, Object);

public:
	HashMap<StringName, Variant> values;
	List<PropertyInfo> properties;
	Ref<ResourceImporter> importer;
	Vector<String> paths;
	HashSet<StringName> checked;
	bool checking = false;
	bool skip = false;
	String base_options_path;

	bool _set(const StringName &p_name, const Variant &p_value) {
		if (values.has(p_name)) {
			values[p_name] = p_value;
			if (checking) {
				checked.insert(p_name);
				notify_property_list_changed();
			}
			return true;
		}

		return false;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {
		if (values.has(p_name)) {
			r_ret = values[p_name];
			return true;
		}

		return false;
	}
	void _get_property_list(List<PropertyInfo> *p_list) const {
		for (const PropertyInfo &E : properties) {
			if (!importer->get_option_visibility(base_options_path, E.name, values)) {
				continue;
			}
			PropertyInfo pi = E;
			if (checking) {
				pi.usage |= PROPERTY_USAGE_CHECKABLE;
				if (checked.has(E.name)) {
					pi.usage |= PROPERTY_USAGE_CHECKED;
				}
			}
			p_list->push_back(pi);
		}
	}

	void update() {
		notify_property_list_changed();
	}
};

ImportDock *ImportDock::singleton = nullptr;

void ImportDock::set_edit_path(const String &p_path) {
	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(p_path + ".import");
	if (err != OK) {
		clear();
		return;
	}

	String importer_name = config->get_value("remap", "importer");
	if (importer_name == "keep") {
		params->importer.unref();
		params->skip = false;
	} else if (importer_name == "skip") {
		params->importer.unref();
		params->skip = true;
	} else {
		params->importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);
		params->skip = false;
	}

	params->paths.clear();
	params->paths.push_back(p_path);
	params->base_options_path = p_path;

	_update_options(p_path, config);

	List<Ref<ResourceImporter>> importers;
	ResourceFormatImporter::get_singleton()->get_importers_for_extension(p_path.get_extension(), &importers);
	List<Pair<String, String>> importer_names;

	for (const Ref<ResourceImporter> &E : importers) {
		importer_names.push_back(Pair<String, String>(E->get_visible_name(), E->get_importer_name()));
	}

	importer_names.sort_custom<PairSort<String, String>>();

	import_as->clear();

	for (const Pair<String, String> &E : importer_names) {
		import_as->add_item(E.first);
		import_as->set_item_metadata(-1, E.second);
		if (E.second == importer_name) {
			import_as->select(import_as->get_item_count() - 1);
		}
	}

	_add_keep_import_option(importer_name);

	import->set_disabled(false);
	_set_dirty(false);
	import_as->set_disabled(false);
	preset->set_disabled(false);
	content->show();
	select_a_resource->hide();

	imported->set_text(p_path.get_file());
}

void ImportDock::_add_keep_import_option(const String &p_importer_name) {
	import_as->add_separator();
	import_as->add_item(TTR("Keep File (exported as is)"));
	import_as->set_item_metadata(-1, "keep");
	import_as->add_item(TTR("Skip File (not exported)"));
	import_as->set_item_metadata(-1, "skip");
	if (p_importer_name == "keep") {
		import_as->select(import_as->get_item_count() - 2);
	} else if (p_importer_name == "skip") {
		import_as->select(import_as->get_item_count() - 1);
	}
}

void ImportDock::_update_options(const String &p_path, const Ref<ConfigFile> &p_config) {
	// Set the importer class to fetch the correct class in the XML class reference.
	// This allows tooltips to display when hovering properties.
	if (params->importer.is_valid()) {
		// Null check to avoid crashing if the "Keep File (exported as is)" mode is selected.
		import_opts->set_object_class(params->importer->get_class_name());
	}

	List<ResourceImporter::ImportOption> options;

	if (params->importer.is_valid()) {
		params->importer->get_import_options(p_path, &options);
	}

	params->properties.clear();
	params->values.clear();
	params->checking = params->paths.size() > 1;
	params->checked.clear();
	params->base_options_path = p_path;

	HashMap<StringName, Variant> import_options;
	if (p_config.is_valid() && p_config->has_section("params")) {
		List<String> section_keys;
		p_config->get_section_keys("params", &section_keys);
		for (const String &section_key : section_keys) {
			import_options[section_key] = p_config->get_value("params", section_key);
		}
		if (params->importer.is_valid()) {
			params->importer->handle_compatibility_options(import_options);
		}
	}

	for (const ResourceImporter::ImportOption &E : options) {
		params->properties.push_back(E.option);
		if (p_config.is_valid() && import_options.has(E.option.name)) {
			params->values[E.option.name] = import_options[E.option.name];
		} else {
			params->values[E.option.name] = E.default_value;
		}
	}

	params->update();
	_update_preset_menu();

	bool was_imported = p_config.is_valid() && p_config->get_value("remap", "importer") != "skip" && p_config->get_value("remap", "importer") != "keep";
	if (was_imported && params->importer.is_valid() && params->paths.size() == 1 && params->importer->has_advanced_options()) {
		advanced->show();
		advanced_spacer->show();
	} else {
		advanced->hide();
		advanced_spacer->hide();
	}
}

void ImportDock::set_edit_multiple_paths(const Vector<String> &p_paths) {
	clear();

	// Use the value that is repeated the most.
	HashMap<String, Dictionary> value_frequency;
	HashSet<String> extensions;

	for (int i = 0; i < p_paths.size(); i++) {
		Ref<ConfigFile> config;
		config.instantiate();
		extensions.insert(p_paths[i].get_extension());
		Error err = config->load(p_paths[i] + ".import");
		ERR_CONTINUE(err != OK);

		if (i == 0) {
			String importer_name = config->get_value("remap", "importer");
			if (importer_name == "keep") {
				params->importer.unref();
				params->skip = false;
			} else if (importer_name == "skip") {
				params->importer.unref();
				params->skip = true;
			} else {
				params->importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);
				params->skip = false;
			}
			if (params->importer.is_null()) {
				clear();
				return;
			}
		}

		if (!config->has_section("params")) {
			continue;
		}

		List<String> keys;
		config->get_section_keys("params", &keys);

		for (const String &E : keys) {
			if (!value_frequency.has(E)) {
				value_frequency[E] = Dictionary();
			}

			Variant value = config->get_value("params", E);

			if (value_frequency[E].has(value)) {
				value_frequency[E][value] = int(value_frequency[E][value]) + 1;
			} else {
				value_frequency[E][value] = 1;
			}
		}
	}

	ERR_FAIL_COND(params->importer.is_null());

	String base_path;
	if (extensions.size() == 1 && p_paths.size() > 0) {
		base_path = p_paths[0];
	}
	List<ResourceImporter::ImportOption> options;
	params->importer->get_import_options(base_path, &options);

	params->properties.clear();
	params->values.clear();
	params->checking = true;
	params->checked.clear();
	params->base_options_path = base_path;

	for (const ResourceImporter::ImportOption &E : options) {
		params->properties.push_back(E.option);

		if (value_frequency.has(E.option.name)) {
			Dictionary d = value_frequency[E.option.name];
			int freq = 0;
			List<Variant> v;
			d.get_key_list(&v);
			Variant value;
			for (const Variant &F : v) {
				int f = d[F];
				if (f > freq) {
					value = F;
				}
			}

			params->values[E.option.name] = value;
		} else {
			params->values[E.option.name] = E.default_value;
		}
	}

	params->update();

	List<Ref<ResourceImporter>> importers;
	ResourceFormatImporter::get_singleton()->get_importers_for_extension(p_paths[0].get_extension(), &importers);
	List<Pair<String, String>> importer_names;

	for (const Ref<ResourceImporter> &E : importers) {
		importer_names.push_back(Pair<String, String>(E->get_visible_name(), E->get_importer_name()));
	}

	importer_names.sort_custom<PairSort<String, String>>();

	import_as->clear();

	for (const Pair<String, String> &E : importer_names) {
		import_as->add_item(E.first);
		import_as->set_item_metadata(-1, E.second);
		if (E.second == params->importer->get_importer_name()) {
			import_as->select(import_as->get_item_count() - 1);
		}
	}

	_add_keep_import_option(params->importer->get_importer_name());

	_update_preset_menu();

	params->paths = p_paths;
	import->set_disabled(false);
	_set_dirty(false);
	import_as->set_disabled(false);
	preset->set_disabled(false);
	content->show();
	select_a_resource->hide();

	imported->set_text(vformat(TTR("%d Files"), p_paths.size()));

	if (params->paths.size() == 1 && params->importer->has_advanced_options()) {
		advanced->show();
		advanced_spacer->show();
	} else {
		advanced->hide();
		advanced_spacer->hide();
	}
}

void ImportDock::reimport_resources(const Vector<String> &p_paths) {
	switch (p_paths.size()) {
		case 0:
			ERR_FAIL_MSG("You need to select files to reimport them.");
		case 1:
			set_edit_path(p_paths[0]);
			break;
		default:
			set_edit_multiple_paths(p_paths);
			break;
	}

	_reimport_attempt();
}

void ImportDock::_update_preset_menu() {
	preset->get_popup()->clear();

	if (params->importer.is_null()) {
		preset->get_popup()->add_item(TTR("Default"));
		preset->hide();
		return;
	}
	preset->show();

	if (params->importer->get_preset_count() == 0) {
		preset->get_popup()->add_item(TTR("Default"));
	} else {
		for (int i = 0; i < params->importer->get_preset_count(); i++) {
			preset->get_popup()->add_item(params->importer->get_preset_name(i));
		}
	}

	preset->get_popup()->add_separator();
	preset->get_popup()->add_item(vformat(TTR("Set as Default for '%s'"), params->importer->get_visible_name()), ITEM_SET_AS_DEFAULT);
	if (ProjectSettings::get_singleton()->has_setting("importer_defaults/" + params->importer->get_importer_name())) {
		preset->get_popup()->add_item(TTR("Load Default"), ITEM_LOAD_DEFAULT);
		preset->get_popup()->add_separator();
		preset->get_popup()->add_item(vformat(TTR("Clear Default for '%s'"), params->importer->get_visible_name()), ITEM_CLEAR_DEFAULT);
	}
}

void ImportDock::_importer_selected(int i_idx) {
	String name = import_as->get_selected_metadata();
	if (name == "keep") {
		params->importer.unref();
		params->skip = false;
		_update_options(params->base_options_path, Ref<ConfigFile>());
	} else if (name == "skip") {
		params->importer.unref();
		params->skip = true;
		_update_options(params->base_options_path, Ref<ConfigFile>());
	} else {
		Ref<ResourceImporter> importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(name);
		ERR_FAIL_COND(importer.is_null());

		params->importer = importer;
		params->skip = false;
		Ref<ConfigFile> config;
		if (params->paths.size()) {
			String path = params->paths[0];
			config.instantiate();
			Error err = config->load(path + ".import");
			if (err != OK) {
				config.unref();
			}
		}
		_update_options(params->base_options_path, config);
	}
}

void ImportDock::_preset_selected(int p_idx) {
	int item_id = preset->get_popup()->get_item_id(p_idx);
	String setting_name = "importer_defaults/" + params->importer->get_importer_name();

	switch (item_id) {
		case ITEM_SET_AS_DEFAULT: {
			Dictionary import_settings;
			// When import settings already exist, we will update these settings
			// to ensure that the dictionary retains settings that are not displayed in the
			// editor. For Scene, the dictionary is the same for FBX, GLTF, and Blender, but each
			// file type has some different settings.
			if (ProjectSettings::get_singleton()->has_setting(setting_name)) {
				import_settings = GLOBAL_GET(setting_name);
			}

			for (const PropertyInfo &E : params->properties) {
				import_settings[E.name] = params->values[E.name];
			}

			ProjectSettings::get_singleton()->set(setting_name, import_settings);
			ProjectSettings::get_singleton()->save();
			_update_preset_menu();
		} break;
		case ITEM_LOAD_DEFAULT: {
			ERR_FAIL_COND(!ProjectSettings::get_singleton()->has_setting(setting_name));

			Dictionary import_settings = GLOBAL_GET(setting_name);
			List<Variant> keys;
			import_settings.get_key_list(&keys);

			if (params->checking) {
				params->checked.clear();
			}
			for (const Variant &E : keys) {
				params->values[E] = import_settings[E];
				if (params->checking) {
					params->checked.insert(E);
				}
			}
			params->update();
		} break;
		case ITEM_CLEAR_DEFAULT: {
			ProjectSettings::get_singleton()->set(setting_name, Variant());
			ProjectSettings::get_singleton()->save();
			_update_preset_menu();
		} break;
		default: {
			List<ResourceImporter::ImportOption> options;

			params->importer->get_import_options(params->base_options_path, &options, p_idx);

			if (params->checking) {
				params->checked.clear();
			}
			for (const ResourceImporter::ImportOption &E : options) {
				params->values[E.option.name] = E.default_value;
				if (params->checking) {
					params->checked.insert(E.option.name);
				}
			}
			params->update();
		} break;
	}
}

void ImportDock::clear() {
	imported->set_text("");
	import->set_disabled(true);
	import_as->clear();
	import_as->set_disabled(true);
	preset->set_disabled(true);
	params->values.clear();
	params->properties.clear();
	params->update();
	preset->get_popup()->clear();
	content->hide();
	select_a_resource->show();
}

static bool _find_owners(EditorFileSystemDirectory *efsd, const String &p_path) {
	if (!efsd) {
		return false;
	}

	for (int i = 0; i < efsd->get_subdir_count(); i++) {
		if (_find_owners(efsd->get_subdir(i), p_path)) {
			return true;
		}
	}

	for (int i = 0; i < efsd->get_file_count(); i++) {
		Vector<String> deps = efsd->get_file_deps(i);
		if (deps.has(p_path)) {
			return true;
		}
	}

	return false;
}

void ImportDock::_reimport_pressed() {
	_reimport_attempt();

	if (params->importer.is_valid() && params->paths.size() == 1 && params->importer->has_advanced_options()) {
		advanced->show();
		advanced_spacer->show();
	} else {
		advanced->hide();
		advanced_spacer->hide();
	}
}

void ImportDock::_reimport_attempt() {
	bool used_in_resources = false;

	String importer_name;
	if (params->importer.is_valid()) {
		importer_name = params->importer->get_importer_name();
	} else {
		if (params->skip) {
			importer_name = "skip";
		} else {
			importer_name = "keep";
		}
	}
	for (int i = 0; i < params->paths.size(); i++) {
		Ref<ConfigFile> config;
		config.instantiate();
		Error err = config->load(params->paths[i] + ".import");
		ERR_CONTINUE(err != OK);

		String imported_with = config->get_value("remap", "importer");
		if (imported_with != importer_name && imported_with != "keep" && imported_with != "skip") {
			Ref<Resource> resource = ResourceLoader::load(params->paths[i]);
			if (resource.is_valid()) {
				need_cleanup.push_back(params->paths[i]);
				if (_find_owners(EditorFileSystem::get_singleton()->get_filesystem(), params->paths[i])) {
					used_in_resources = true;
				}
			}
		}
	}

	if (!need_cleanup.is_empty() || used_in_resources) {
		cleanup_warning->set_visible(!need_cleanup.is_empty());
		label_warning->set_visible(used_in_resources);
		reimport_confirm->popup_centered();
		return;
	}

	_reimport();
}

void ImportDock::_reimport_and_cleanup() {
	HashMap<String, Ref<Resource>> old_resources;

	for (const String &path : need_cleanup) {
		Ref<Resource> res = ResourceLoader::load(path);
		res->set_path("");
		res->set_meta(SNAME("_skip_save_"), true);
		old_resources[path] = res;
	}

	EditorResourcePreview::get_singleton()->stop(); // Don't try to re-create previews after import.
	_reimport();

	if (need_cleanup.is_empty()) {
		return;
	}

	// After changing resource type we need to make sure that all old instances are unloaded or replaced.
	EditorNode::get_singleton()->push_item(nullptr);
	EditorUndoRedoManager::get_singleton()->clear_history();

	List<Ref<Resource>> external_resources;
	ResourceCache::get_cached_resources(&external_resources);

	Vector<Ref<Resource>> old_resources_to_replace;
	Vector<Ref<Resource>> new_resources_to_replace;
	for (const String &path : need_cleanup) {
		Ref<Resource> old_res = old_resources[path];
		if (params->importer.is_valid()) {
			Ref<Resource> new_res = ResourceLoader::load(path);
			if (new_res.is_valid()) {
				old_resources_to_replace.append(old_res);
				new_resources_to_replace.append(new_res);
			}
		}
	}

	EditorNode::get_singleton()->replace_resources_in_scenes(old_resources_to_replace, new_resources_to_replace);

	for (Ref<Resource> res : external_resources) {
		EditorNode::get_singleton()->replace_resources_in_object(res.ptr(), old_resources_to_replace, new_resources_to_replace);
	}

	need_cleanup.clear();
}

void ImportDock::_advanced_options() {
	if (params->paths.size() == 1 && params->importer.is_valid()) {
		params->importer->show_advanced_options(params->paths[0]);
	}
}

void ImportDock::_reimport() {
	for (int i = 0; i < params->paths.size(); i++) {
		Ref<ConfigFile> config;
		config.instantiate();
		Error err = config->load(params->paths[i] + ".import");
		ERR_CONTINUE(err != OK);

		if (params->importer.is_valid()) {
			String importer_name = params->importer->get_importer_name();

			if (params->checking && config->get_value("remap", "importer") == params->importer->get_importer_name()) {
				//update only what is edited (checkboxes) if the importer is the same
				for (const PropertyInfo &E : params->properties) {
					if (params->checked.has(E.name)) {
						config->set_value("params", E.name, params->values[E.name]);
					}
				}
			} else {
				//override entirely
				config->set_value("remap", "importer", importer_name);
				if (config->has_section("params")) {
					config->erase_section("params");
				}

				for (const PropertyInfo &E : params->properties) {
					config->set_value("params", E.name, params->values[E.name]);
				}
			}

			//handle group file
			Ref<ResourceImporter> importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);
			ERR_CONTINUE(importer.is_null());
			String group_file_property = importer->get_option_group_file();
			if (!group_file_property.is_empty()) {
				//can import from a group (as in, atlas)
				ERR_CONTINUE(!params->values.has(group_file_property));
				String group_file = params->values[group_file_property];
				config->set_value("remap", "group_file", group_file);
			} else {
				config->set_value("remap", "group_file", Variant()); //clear group file if unused
			}

		} else {
			//set to no import
			config->clear();
			if (params->skip) {
				config->set_value("remap", "importer", "skip");
			} else {
				config->set_value("remap", "importer", "keep");
			}
		}

		config->save(params->paths[i] + ".import");
	}

	EditorFileSystem::get_singleton()->reimport_files(params->paths);
	EditorFileSystem::get_singleton()->emit_signal(SNAME("filesystem_changed")); //it changed, so force emitting the signal

	_set_dirty(false);
}

void ImportDock::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorThemeManager::is_generated_theme_outdated()) {
				imported->add_theme_style_override(CoreStringName(normal), get_theme_stylebox(CoreStringName(normal), SNAME("LineEdit")));
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			import_opts->edit(params);
			label_warning->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		} break;
	}
}

void ImportDock::_property_edited(const StringName &p_prop) {
	_set_dirty(true);
}

void ImportDock::_set_dirty(bool p_dirty) {
	if (p_dirty) {
		// Add a dirty marker to notify the user that they should reimport the selected resource to see changes.
		import->set_text(TTR("Reimport") + " (*)");
		import->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		import->set_tooltip_text(TTR("You have pending changes that haven't been applied yet. Click Reimport to apply changes made to the import options.\nSelecting another resource in the FileSystem dock without clicking Reimport first will discard changes made in the Import dock."));
	} else {
		// Remove the dirty marker on the Reimport button.
		import->set_text(TTR("Reimport"));
		import->remove_theme_color_override(SceneStringName(font_color));
		import->set_tooltip_text("");
	}
}

void ImportDock::_property_toggled(const StringName &p_prop, bool p_checked) {
	if (p_checked) {
		params->checked.insert(p_prop);
	} else {
		params->checked.erase(p_prop);
	}
}

void ImportDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_reimport"), &ImportDock::_reimport);
}

void ImportDock::initialize_import_options() const {
	ERR_FAIL_COND(!import_opts || !params);

	import_opts->edit(params);
}

ImportDock::ImportDock() {
	singleton = this;
	set_name("Import");

	content = memnew(VBoxContainer);
	content->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(content);
	content->hide();

	imported = memnew(Label);
	imported->add_theme_style_override(CoreStringName(normal), EditorNode::get_singleton()->get_editor_theme()->get_stylebox(CoreStringName(normal), SNAME("LineEdit")));
	imported->set_clip_text(true);
	content->add_child(imported);
	HBoxContainer *hb = memnew(HBoxContainer);
	content->add_margin_child(TTR("Import As:"), hb);
	import_as = memnew(OptionButton);
	import_as->set_disabled(true);
	import_as->set_fit_to_longest_item(false);
	import_as->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	import_as->set_h_size_flags(SIZE_EXPAND_FILL);
	import_as->connect(SceneStringName(item_selected), callable_mp(this, &ImportDock::_importer_selected));
	hb->add_child(import_as);
	import_as->set_h_size_flags(SIZE_EXPAND_FILL);
	preset = memnew(MenuButton);
	preset->set_text(TTR("Preset"));
	preset->set_disabled(true);
	preset->get_popup()->connect("index_pressed", callable_mp(this, &ImportDock::_preset_selected));
	hb->add_child(preset);

	import_opts = memnew(EditorInspector);
	content->add_child(import_opts);
	import_opts->set_v_size_flags(SIZE_EXPAND_FILL);
	import_opts->connect("property_edited", callable_mp(this, &ImportDock::_property_edited));
	import_opts->connect("property_toggled", callable_mp(this, &ImportDock::_property_toggled));
	// Make it possible to display tooltips stored in the XML class reference.
	// The object name is set when the importer changes in `_update_options()`.
	import_opts->set_use_doc_hints(true);

	hb = memnew(HBoxContainer);
	content->add_child(hb);
	import = memnew(Button);
	import->set_text(TTR("Reimport"));
	import->set_disabled(true);
	import->connect(SceneStringName(pressed), callable_mp(this, &ImportDock::_reimport_pressed));
	advanced_spacer = hb->add_spacer();
	advanced = memnew(Button);
	advanced->set_text(TTR("Advanced..."));
	hb->add_child(advanced);
	hb->add_spacer();
	hb->add_child(import);
	hb->add_spacer();

	advanced->hide();
	advanced_spacer->hide();
	advanced->connect(SceneStringName(pressed), callable_mp(this, &ImportDock::_advanced_options));

	reimport_confirm = memnew(ConfirmationDialog);
	content->add_child(reimport_confirm);
	reimport_confirm->connect(SceneStringName(confirmed), callable_mp(this, &ImportDock::_reimport_and_cleanup));

	VBoxContainer *vbc_confirm = memnew(VBoxContainer());
	cleanup_warning = memnew(Label(TTR("The imported resource is currently loaded. All instances will be replaced and undo history will be cleared.")));
	vbc_confirm->add_child(cleanup_warning);
	label_warning = memnew(Label(TTR("WARNING: Assets exist that use this resource. They may stop loading properly after changing type.")));
	vbc_confirm->add_child(label_warning);
	reimport_confirm->add_child(vbc_confirm);

	params = memnew(ImportDockParameters);

	select_a_resource = memnew(Label);
	select_a_resource->set_text(TTR("Select a resource file in the filesystem or in the inspector to adjust import settings."));
	select_a_resource->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	select_a_resource->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	select_a_resource->set_v_size_flags(SIZE_EXPAND_FILL);
	select_a_resource->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	select_a_resource->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	add_child(select_a_resource);
}

ImportDock::~ImportDock() {
	singleton = nullptr;
	memdelete(params);
}
