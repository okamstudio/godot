/**************************************************************************/
/*  script_create_dialog.cpp                                              */
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

#include "script_create_dialog.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "core/string/string_builder.h"
#include "editor/create_dialog.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/line_edit.h"
#include "scene/theme/theme_db.h"

static String _get_parent_class_of_script(const String &p_path) {
	if (!ResourceLoader::exists(p_path, "Script")) {
		return "Object"; // A script eventually inherits from Object.
	}

	Ref<Script> script = ResourceLoader::load(p_path, "Script");
	ERR_FAIL_COND_V(script.is_null(), "Object");

	String class_name;
	Ref<Script> base = script->get_base_script();

	// Inherits from a built-in class.
	if (base.is_null()) {
		script->get_language()->get_global_class_name(script->get_path(), &class_name);
		return class_name;
	}

	// Inherits from a script that has class_name.
	class_name = script->get_language()->get_global_class_name(base->get_path());
	if (!class_name.is_empty()) {
		return class_name;
	}

	// Inherits from a plain script.
	return _get_parent_class_of_script(base->get_path());
}

static Vector<String> _get_hierarchy(const String &p_class_name) {
	Vector<String> hierarchy;

	String class_name = p_class_name;
	while (true) {
		// A registered class.
		if (ClassDB::class_exists(class_name)) {
			hierarchy.push_back(class_name);

			class_name = ClassDB::get_parent_class(class_name);
			continue;
		}

		// A class defined in script with class_name.
		if (ScriptServer::is_global_class(class_name)) {
			hierarchy.push_back(class_name);

			Ref<Script> script = EditorNode::get_editor_data().script_class_load_script(class_name);
			ERR_BREAK(script.is_null());
			class_name = _get_parent_class_of_script(script->get_path());
			continue;
		}

		break;
	}

	if (hierarchy.is_empty()) {
		if (p_class_name.is_valid_ascii_identifier()) {
			hierarchy.push_back(p_class_name);
		}
		hierarchy.push_back("Object");
	}

	return hierarchy;
}

void ScriptCreateDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			String last_language = EditorSettings::get_singleton()->get_project_metadata("script_setup", "last_selected_language", "");
			if (!last_language.is_empty()) {
				for (int i = 0; i < language_menu->get_item_count(); i++) {
					if (language_menu->get_item_text(i) == last_language) {
						language_menu->select(i);
						break;
					}
				}
			} else {
				language_menu->select(default_language);
			}
			is_using_templates = EDITOR_DEF("_script_setup_use_script_templates", false);
			use_templates->set_pressed(is_using_templates);
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			const int icon_size = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));

			EditorData &ed = EditorNode::get_editor_data();

			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				// Check if the extension has an icon first.
				String script_type = ScriptServer::get_language(i)->get_type();
				Ref<Texture2D> language_icon = get_editor_theme_icon(script_type);
				if (language_icon.is_null() || language_icon == ThemeDB::get_singleton()->get_fallback_icon()) {
					// The theme doesn't have an icon for this language, ask the extensions.
					Ref<Texture2D> extension_language_icon = ed.extension_class_get_icon(script_type);
					if (extension_language_icon.is_valid()) {
						language_menu->get_popup()->set_item_icon_max_width(i, icon_size);
						language_icon = extension_language_icon;
					}
				}

				if (language_icon.is_valid()) {
					language_menu->set_item_icon(i, language_icon);
				}
			}

			path_button->set_button_icon(get_editor_theme_icon(SNAME("Folder")));
			parent_browse_button->set_button_icon(get_editor_theme_icon(SNAME("Folder")));
			parent_search_button->set_button_icon(get_editor_theme_icon(SNAME("ClassList")));
		} break;
	}
}

void ScriptCreateDialog::_path_hbox_sorted() {
	if (is_visible()) {
		int filename_start_pos = file_path->get_text().rfind_char('/') + 1;
		int filename_end_pos = file_path->get_text().get_basename().length();

		if (!is_built_in) {
			file_path->select(filename_start_pos, filename_end_pos);
		}

		// First set cursor to the end of line to scroll LineEdit view
		// to the right and then set the actual cursor position.
		file_path->set_caret_column(file_path->get_text().length());
		file_path->set_caret_column(filename_start_pos);

		file_path->grab_focus();
	}
}

bool ScriptCreateDialog::_can_be_built_in() {
	return (supports_built_in && built_in_enabled);
}

String ScriptCreateDialog::_adjust_file_path(const String &p_base_path) const {
	if (p_base_path.is_empty()) {
		return p_base_path;
	}

	String base_dir = p_base_path.get_base_dir();
	String file_name = p_base_path.get_file().get_basename();
	file_name = EditorNode::adjust_script_name_casing(file_name, language->preferred_file_name_casing());
	String extension = language->get_extension();
	return base_dir.path_join(file_name + "." + extension);
}

void ScriptCreateDialog::config(const String &p_base_name, const String &p_base_path, bool p_built_in_enabled, bool p_load_enabled) {
	parent_name->set_text(p_base_name);
	parent_name->deselect();
	built_in_name->set_text("");

	file_path->set_text(p_base_path);
	file_path->deselect();

	built_in_enabled = p_built_in_enabled;
	load_enabled = p_load_enabled;

	_language_changed(language_menu->get_selected());
}

void ScriptCreateDialog::set_inheritance_base_type(const String &p_base) {
	base_type = p_base;
}

bool ScriptCreateDialog::_validate_parent(const String &p_string) {
	if (p_string.length() == 0) {
		return false;
	}

	if (can_inherit_from_file && p_string.is_quoted()) {
		String p = p_string.substr(1, p_string.length() - 2);
		if (_validate_path(p, true) == "") {
			return true;
		}
	}

	return EditorNode::get_editor_data().is_type_recognized(p_string);
}

String ScriptCreateDialog::_validate_path(const String &p_path, bool p_file_must_exist) {
	String p = p_path.strip_edges();

	if (p.is_empty()) {
		return TTR("Path is empty.");
	}
	if (p.get_file().get_basename().is_empty()) {
		return TTR("Filename is empty.");
	}

	if (!p.get_file().get_basename().is_valid_filename()) {
		return TTR("Filename is invalid.");
	}
	if (p.get_file().begins_with(".")) {
		return TTR("Name begins with a dot.");
	}

	p = ProjectSettings::get_singleton()->localize_path(p);
	if (!p.begins_with("res://")) {
		return TTR("Path is not local.");
	}

	{
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (da->change_dir(p.get_base_dir()) != OK) {
			return TTR("Base path is invalid.");
		}
	}

	{
		// Check if file exists.
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (da->dir_exists(p)) {
			return TTR("A directory with the same name exists.");
		} else if (p_file_must_exist && !da->file_exists(p)) {
			return TTR("File does not exist.");
		}
	}

	// Check file extension.
	String extension = p.get_extension();
	List<String> extensions;

	// Get all possible extensions for script.
	for (int l = 0; l < language_menu->get_item_count(); l++) {
		ScriptServer::get_language(l)->get_recognized_extensions(&extensions);
	}

	bool found = false;
	bool match = false;
	for (const String &E : extensions) {
		if (E.nocasecmp_to(extension) == 0) {
			found = true;
			if (E == ScriptServer::get_language(language_menu->get_selected())->get_extension()) {
				match = true;
			}
			break;
		}
	}

	if (!found) {
		return TTR("Invalid extension.");
	}
	if (!match) {
		return TTR("Extension doesn't match chosen language.");
	}

	// Let ScriptLanguage do custom validation.
	return ScriptServer::get_language(language_menu->get_selected())->validate_path(p);
}

void ScriptCreateDialog::_parent_name_changed(const String &p_parent) {
	is_parent_name_valid = _validate_parent(parent_name->get_text());
	validation_panel->update();
}

void ScriptCreateDialog::_template_changed(int p_template) {
	const ScriptLanguage::ScriptTemplate &sinfo = _get_current_template();
	// Update last used dictionaries
	if (is_using_templates && !parent_name->get_text().begins_with("\"res:")) {
		if (sinfo.origin == ScriptLanguage::TemplateLocation::TEMPLATE_PROJECT) {
			// Save the last used template for this node into the project dictionary.
			Dictionary dic_templates_project = EditorSettings::get_singleton()->get_project_metadata("script_setup", "templates_dictionary", Dictionary());
			dic_templates_project[parent_name->get_text()] = sinfo.get_hash();
			EditorSettings::get_singleton()->set_project_metadata("script_setup", "templates_dictionary", dic_templates_project);
		} else {
			// Save template info to editor dictionary (not a project template).
			Dictionary dic_templates = EDITOR_GET("_script_setup_templates_dictionary");
			dic_templates[parent_name->get_text()] = sinfo.get_hash();
			EditorSettings::get_singleton()->set("_script_setup_templates_dictionary", dic_templates);
			// Remove template from project dictionary as we last used an editor level template.
			Dictionary dic_templates_project = EditorSettings::get_singleton()->get_project_metadata("script_setup", "templates_dictionary", Dictionary());
			if (dic_templates_project.has(parent_name->get_text())) {
				dic_templates_project.erase(parent_name->get_text());
				EditorSettings::get_singleton()->set_project_metadata("script_setup", "templates_dictionary", dic_templates_project);
			}
		}
	}

	// Update template label information.
	String template_info = U"•  ";
	template_info += TTR("Template:");
	template_info += " " + sinfo.name;
	if (!sinfo.description.is_empty()) {
		template_info += " - " + sinfo.description;
	}
	validation_panel->set_message(MSG_ID_TEMPLATE, template_info, EditorValidationPanel::MSG_INFO, false);
}

void ScriptCreateDialog::ok_pressed() {
	if (is_new_script_created) {
		_create_new();
	} else {
		_load_exist();
	}

	EditorSettings::get_singleton()->save();
	is_new_script_created = true;
	validation_panel->update();
}

void ScriptCreateDialog::_create_new() {
	Ref<Script> scr;

	const ScriptLanguage::ScriptTemplate sinfo = _get_current_template();

	String parent_class = parent_name->get_text();
	if (!parent_name->get_text().is_quoted() && !ClassDB::class_exists(parent_class) && !ScriptServer::is_global_class(parent_class)) {
		// If base is a custom type, replace with script path instead.
		const EditorData::CustomType *type = EditorNode::get_editor_data().get_custom_type_by_name(parent_class);
		ERR_FAIL_NULL(type);
		parent_class = "\"" + type->script->get_path() + "\"";
	}

	String class_name = file_path->get_text().get_file().get_basename();
	scr = ScriptServer::get_language(language_menu->get_selected())->make_template(sinfo.content, class_name, parent_class);

	if (is_built_in) {
		scr->set_name(built_in_name->get_text());
		// Make sure the script is compiled to make its type recognizable.
		scr->reload();
	} else {
		String lpath = ProjectSettings::get_singleton()->localize_path(file_path->get_text());
		scr->set_path(lpath);
		Error err = ResourceSaver::save(scr, lpath, ResourceSaver::FLAG_CHANGE_PATH);
		if (err != OK) {
			alert->set_text(TTR("Error - Could not create script in filesystem."));
			alert->popup_centered();
			return;
		}
		EditorNode::get_singleton()->ensure_uid_file(lpath);
	}

	emit_signal(SNAME("script_created"), scr);
	hide();
}

void ScriptCreateDialog::_load_exist() {
	String path = file_path->get_text();
	Ref<Resource> p_script = ResourceLoader::load(path, "Script");
	if (p_script.is_null()) {
		alert->set_text(vformat(TTR("Error loading script from %s"), path));
		alert->popup_centered();
		return;
	}

	emit_signal(SNAME("script_created"), p_script);
	hide();
}

void ScriptCreateDialog::_language_changed(int l) {
	language = ScriptServer::get_language(l);

	can_inherit_from_file = language->can_inherit_from_file();
	supports_built_in = language->supports_builtin_mode();
	if (!supports_built_in) {
		is_built_in = false;
	}

	String path = file_path->get_text();
	path = _adjust_file_path(path);
	_path_changed(path);
	file_path->set_text(path);

	EditorSettings::get_singleton()->set_project_metadata("script_setup", "last_selected_language", language_menu->get_item_text(language_menu->get_selected()));

	_parent_name_changed(parent_name->get_text());
	validation_panel->update();
}

void ScriptCreateDialog::_built_in_pressed() {
	if (built_in->is_pressed()) {
		is_built_in = true;
		is_new_script_created = true;
	} else {
		is_built_in = false;
		_path_changed(file_path->get_text());
	}
	validation_panel->update();
}

void ScriptCreateDialog::_use_template_pressed() {
	is_using_templates = use_templates->is_pressed();
	EditorSettings::get_singleton()->set("_script_setup_use_script_templates", is_using_templates);
	validation_panel->update();
}

void ScriptCreateDialog::_browse_path(bool browse_parent, bool p_save) {
	is_browsing_parent = browse_parent;

	if (p_save) {
		file_browse->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
		file_browse->set_title(TTR("Open Script / Choose Location"));
		file_browse->set_ok_button_text(TTR("Open"));
	} else {
		file_browse->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		file_browse->set_title(TTR("Open Script"));
	}

	file_browse->set_disable_overwrite_warning(true);
	file_browse->clear_filters();
	List<String> extensions;

	int lang = language_menu->get_selected();
	ScriptServer::get_language(lang)->get_recognized_extensions(&extensions);

	for (const String &E : extensions) {
		file_browse->add_filter("*." + E);
	}

	file_browse->set_current_path(file_path->get_text());
	file_browse->popup_file_dialog();
}

void ScriptCreateDialog::_file_selected(const String &p_file) {
	String path = ProjectSettings::get_singleton()->localize_path(p_file);
	if (is_browsing_parent) {
		parent_name->set_text("\"" + path + "\"");
		_parent_name_changed(parent_name->get_text());
	} else {
		file_path->set_text(path);
		_path_changed(path);

		String filename = path.get_file().get_basename();
		int select_start = path.rfind(filename);
		file_path->select(select_start, select_start + filename.length());
		file_path->set_caret_column(select_start + filename.length());
		file_path->grab_focus();
	}
}

void ScriptCreateDialog::_create() {
	parent_name->set_text(select_class->get_selected_type().split(" ")[0]);
	_parent_name_changed(parent_name->get_text());
}

void ScriptCreateDialog::_browse_class_in_tree() {
	select_class->set_base_type(base_type);
	select_class->popup_create(true);
	select_class->set_title(vformat(TTR("Inherit %s"), base_type));
	select_class->set_ok_button_text(TTR("Inherit"));
}

void ScriptCreateDialog::_path_changed(const String &p_path) {
	if (is_built_in) {
		return;
	}

	is_path_valid = false;
	is_new_script_created = true;

	path_error = _validate_path(p_path, false);
	if (!path_error.is_empty()) {
		validation_panel->update();
		return;
	}

	// Check if file exists.
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	String p = ProjectSettings::get_singleton()->localize_path(p_path.strip_edges());
	if (da->file_exists(p)) {
		is_new_script_created = false;
	}

	is_path_valid = true;
	validation_panel->update();
}

void ScriptCreateDialog::_update_template_menu() {
	bool is_language_using_templates = language->is_using_templates();
	template_menu->set_disabled(false);
	template_menu->clear();
	template_list.clear();

	if (is_language_using_templates) {
		// Get the latest templates used for each type of node from project settings then global settings.
		Dictionary last_local_templates = EditorSettings::get_singleton()->get_project_metadata("script_setup", "templates_dictionary", Dictionary());
		Dictionary last_global_templates = EDITOR_GET("_script_setup_templates_dictionary");
		String inherits_base_type = parent_name->get_text();

		// If it inherits from a script, get its parent class first.
		if (inherits_base_type[0] == '"') {
			inherits_base_type = _get_parent_class_of_script(inherits_base_type.unquote());
		}

		// Get all ancestor node for selected base node.
		// There templates will also fit the base node.
		Vector<String> hierarchy = _get_hierarchy(inherits_base_type);
		int last_used_template = -1;
		int preselected_template = -1;
		int previous_ancestor_level = -1;

		// Templates can be stored in tree different locations.
		Vector<ScriptLanguage::TemplateLocation> template_locations;
		template_locations.append(ScriptLanguage::TEMPLATE_PROJECT);
		template_locations.append(ScriptLanguage::TEMPLATE_EDITOR);
		template_locations.append(ScriptLanguage::TEMPLATE_BUILT_IN);

		for (const ScriptLanguage::TemplateLocation &template_location : template_locations) {
			String display_name = _get_script_origin_label(template_location);
			bool separator = false;
			int ancestor_level = 0;
			for (const String &current_node : hierarchy) {
				Vector<ScriptLanguage::ScriptTemplate> templates_found;
				if (template_location == ScriptLanguage::TEMPLATE_BUILT_IN) {
					templates_found = language->get_built_in_templates(current_node);
				} else {
					String template_directory;
					if (template_location == ScriptLanguage::TEMPLATE_PROJECT) {
						template_directory = EditorPaths::get_singleton()->get_project_script_templates_dir();
					} else {
						template_directory = EditorPaths::get_singleton()->get_script_templates_dir();
					}
					templates_found = _get_user_templates(language, current_node, template_directory, template_location);
				}
				if (!templates_found.is_empty()) {
					if (!separator) {
						template_menu->add_separator();
						template_menu->set_item_text(-1, display_name);
						separator = true;
					}
					for (ScriptLanguage::ScriptTemplate &t : templates_found) {
						template_menu->add_item(t.inherit + ": " + t.name);
						int id = template_menu->get_item_count() - 1;
						// Check if this template should be preselected if node isn't in the last used dictionary.
						if (ancestor_level < previous_ancestor_level || previous_ancestor_level == -1) {
							previous_ancestor_level = ancestor_level;
							preselected_template = id;
						}
						// Check for last used template for this node in project settings then in global settings.
						if (last_local_templates.has(parent_name->get_text()) && t.get_hash() == String(last_local_templates[parent_name->get_text()])) {
							last_used_template = id;
						} else if (last_used_template == -1 && last_global_templates.has(parent_name->get_text()) && t.get_hash() == String(last_global_templates[parent_name->get_text()])) {
							last_used_template = id;
						}
						t.id = id;
						template_list.push_back(t);
						String icon = has_theme_icon(t.inherit, EditorStringName(EditorIcons)) ? t.inherit : "Object";
						template_menu->set_item_icon(id, get_editor_theme_icon(icon));
					}
				}
				ancestor_level++;
			}
		}

		if (last_used_template != -1) {
			template_menu->select(last_used_template);
		} else if (preselected_template != -1) {
			template_menu->select(preselected_template);
		}
	}
	_template_changed(template_menu->get_selected());
}

void ScriptCreateDialog::_update_dialog() {
	// "Add Script Dialog" GUI logic and script checks.
	_update_template_menu();

	// Is script path/name valid (order from top to bottom)?

	if (!is_built_in && !is_path_valid) {
		validation_panel->set_message(MSG_ID_SCRIPT, TTR("Invalid path."), EditorValidationPanel::MSG_ERROR);
	}

	if (!is_parent_name_valid && is_new_script_created) {
		validation_panel->set_message(MSG_ID_SCRIPT, TTR("Invalid inherited parent name or path."), EditorValidationPanel::MSG_ERROR);
	}

	if (validation_panel->is_valid() && !is_new_script_created) {
		validation_panel->set_message(MSG_ID_SCRIPT, TTR("File exists, it will be reused."), EditorValidationPanel::MSG_OK);
	}

	if (!is_built_in && !path_error.is_empty()) {
		validation_panel->set_message(MSG_ID_PATH, path_error, EditorValidationPanel::MSG_ERROR);
	}

	// Is script Built-in?

	if (is_built_in) {
		file_path->set_editable(false);
		path_button->set_disabled(true);
		re_check_path = true;
	} else {
		file_path->set_editable(true);
		path_button->set_disabled(false);
		if (re_check_path) {
			re_check_path = false;
			_path_changed(file_path->get_text());
		}
	}

	if (!_can_be_built_in()) {
		built_in->set_pressed(false);
	}
	built_in->set_disabled(!_can_be_built_in());

	// Is Script created or loaded from existing file?

	if (is_built_in) {
		validation_panel->set_message(MSG_ID_BUILT_IN, TTR("Note: Built-in scripts have some limitations and can't be edited using an external editor."), EditorValidationPanel::MSG_INFO, false);
	} else if (file_path->get_text().get_file().get_basename() == parent_name->get_text()) {
		validation_panel->set_message(MSG_ID_BUILT_IN, TTR("Warning: Having the script name be the same as a built-in type is usually not desired."), EditorValidationPanel::MSG_WARNING, false);
	}

	path_controls[0]->set_visible(!is_built_in);
	path_controls[1]->set_visible(!is_built_in);
	name_controls[0]->set_visible(is_built_in);
	name_controls[1]->set_visible(is_built_in);

	bool is_new_file = is_built_in || is_new_script_created;

	parent_name->set_editable(is_new_file);
	parent_search_button->set_disabled(!is_new_file);
	parent_browse_button->set_disabled(!is_new_file || !can_inherit_from_file);
	template_inactive_message = "";
	String button_text = is_new_file ? TTR("Create") : TTR("Load");
	set_ok_button_text(button_text);

	if (is_new_file) {
		if (is_built_in) {
			validation_panel->set_message(MSG_ID_PATH, TTR("Built-in script (into scene file)."), EditorValidationPanel::MSG_OK);
		}
	} else {
		template_inactive_message = TTR("Using existing script file.");
		if (load_enabled) {
			if (is_path_valid) {
				validation_panel->set_message(MSG_ID_PATH, TTR("Will load an existing script file."), EditorValidationPanel::MSG_OK);
			}
		} else {
			validation_panel->set_message(MSG_ID_PATH, TTR("Script file already exists."), EditorValidationPanel::MSG_ERROR);
		}
	}

	// Show templates list if needed.
	if (is_using_templates) {
		// Check if at least one suitable template has been found.
		if (template_menu->get_item_count() == 0 && template_inactive_message.is_empty()) {
			template_inactive_message = TTR("No suitable template.");
		}
	} else {
		template_inactive_message = TTR("Empty");
	}

	if (!template_inactive_message.is_empty()) {
		template_menu->set_disabled(true);
		template_menu->clear();
		template_menu->add_item(template_inactive_message);
		validation_panel->set_message(MSG_ID_TEMPLATE, "", EditorValidationPanel::MSG_INFO);
	}
}

ScriptLanguage::ScriptTemplate ScriptCreateDialog::_get_current_template() const {
	int selected_index = template_menu->get_selected();
	for (const ScriptLanguage::ScriptTemplate &t : template_list) {
		if (is_using_templates) {
			if (t.id == selected_index) {
				return t;
			}
		} else {
			// Using empty built-in template if templates are disabled.
			if (t.origin == ScriptLanguage::TemplateLocation::TEMPLATE_BUILT_IN && t.name == "Empty") {
				return t;
			}
		}
	}
	return ScriptLanguage::ScriptTemplate();
}

Vector<ScriptLanguage::ScriptTemplate> ScriptCreateDialog::_get_user_templates(const ScriptLanguage *p_language, const StringName &p_object, const String &p_dir, const ScriptLanguage::TemplateLocation &p_origin) const {
	Vector<ScriptLanguage::ScriptTemplate> user_templates;
	String extension = p_language->get_extension();

	String dir_path = p_dir.path_join(p_object);

	Ref<DirAccess> d = DirAccess::open(dir_path);
	if (d.is_valid()) {
		d->list_dir_begin();
		String file = d->get_next();
		while (file != String()) {
			if (file.get_extension() == extension) {
				user_templates.append(_parse_template(p_language, dir_path, file, p_origin, p_object));
			}
			file = d->get_next();
		}
		d->list_dir_end();
	}
	return user_templates;
}

ScriptLanguage::ScriptTemplate ScriptCreateDialog::_parse_template(const ScriptLanguage *p_language, const String &p_path, const String &p_filename, const ScriptLanguage::TemplateLocation &p_origin, const String &p_inherits) const {
	ScriptLanguage::ScriptTemplate script_template = ScriptLanguage::ScriptTemplate();
	script_template.origin = p_origin;
	script_template.inherit = p_inherits;
	int space_indent_size = 4;
	// Get meta delimiter
	String meta_delimiter;
	List<String> comment_delimiters;
	p_language->get_comment_delimiters(&comment_delimiters);
	for (const String &script_delimiter : comment_delimiters) {
		if (!script_delimiter.contains_char(' ')) {
			meta_delimiter = script_delimiter;
			break;
		}
	}
	String meta_prefix = meta_delimiter + " meta-";

	// Parse file for meta-information and script content
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path.path_join(p_filename), FileAccess::READ, &err);
	if (!err) {
		while (!file->eof_reached()) {
			String line = file->get_line();
			if (line.begins_with(meta_prefix)) {
				// Store meta information
				line = line.substr(meta_prefix.length());
				if (line.begins_with("name:")) {
					script_template.name = line.substr(5).strip_edges();
				} else if (line.begins_with("description:")) {
					script_template.description = line.substr(12).strip_edges();
				} else if (line.begins_with("space-indent:")) {
					String indent_value = line.substr(13).strip_edges();
					if (indent_value.is_valid_int()) {
						int indent_size = indent_value.to_int();
						if (indent_size >= 0) {
							space_indent_size = indent_size;
						} else {
							WARN_PRINT(vformat("Template meta-space-indent need to be a non-negative integer value. Found %s.", indent_value));
						}
					} else {
						WARN_PRINT(vformat("Template meta-space-indent need to be a valid integer value. Found %s.", indent_value));
					}
				}
			} else {
				// Replace indentation.
				int i = 0;
				int space_count = 0;
				for (; i < line.length(); i++) {
					if (line[i] == '\t') {
						if (space_count) {
							script_template.content += String(" ").repeat(space_count);
							space_count = 0;
						}
						script_template.content += "_TS_";
					} else if (line[i] == ' ') {
						space_count++;
						if (space_count == space_indent_size) {
							script_template.content += "_TS_";
							space_count = 0;
						}
					} else {
						break;
					}
				}
				if (space_count) {
					script_template.content += String(" ").repeat(space_count);
				}
				script_template.content += line.substr(i) + "\n";
			}
		}
	}

	script_template.content = script_template.content.lstrip("\n");

	// Get name from file name if no name in meta information
	if (script_template.name == String()) {
		script_template.name = p_filename.get_basename().capitalize();
	}

	return script_template;
}

String ScriptCreateDialog::_get_script_origin_label(const ScriptLanguage::TemplateLocation &p_origin) const {
	switch (p_origin) {
		case ScriptLanguage::TEMPLATE_BUILT_IN:
			return TTR("Built-in");
		case ScriptLanguage::TEMPLATE_EDITOR:
			return TTR("Editor");
		case ScriptLanguage::TEMPLATE_PROJECT:
			return TTR("Project");
	}
	return "";
}

void ScriptCreateDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("config", "inherits", "path", "built_in_enabled", "load_enabled"), &ScriptCreateDialog::config, DEFVAL(true), DEFVAL(true));

	ADD_SIGNAL(MethodInfo("script_created", PropertyInfo(Variant::OBJECT, "script", PROPERTY_HINT_RESOURCE_TYPE, "Script")));
}

ScriptCreateDialog::ScriptCreateDialog() {
	EDITOR_DEF("_script_setup_templates_dictionary", Dictionary());

	/* Main Controls */

	GridContainer *gc = memnew(GridContainer);
	gc->set_columns(2);

	/* Information Messages Field */

	validation_panel = memnew(EditorValidationPanel);
	validation_panel->add_line(MSG_ID_SCRIPT, TTR("Script path/name is valid."));
	validation_panel->add_line(MSG_ID_PATH, TTR("Will create a new script file."));
	validation_panel->add_line(MSG_ID_BUILT_IN);
	validation_panel->add_line(MSG_ID_TEMPLATE);
	validation_panel->set_update_callback(callable_mp(this, &ScriptCreateDialog::_update_dialog));
	validation_panel->set_accept_button(get_ok_button());

	/* Spacing */

	Control *spacing = memnew(Control);
	spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->add_child(gc);
	vb->add_child(spacing);
	vb->add_child(validation_panel);
	add_child(vb);

	/* Language */

	language_menu = memnew(OptionButton);
	language_menu->set_custom_minimum_size(Size2(350, 0) * EDSCALE);
	language_menu->set_expand_icon(true);
	language_menu->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	gc->add_child(memnew(Label(TTR("Language:"))));
	gc->add_child(language_menu);

	default_language = -1;
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		String lang = ScriptServer::get_language(i)->get_name();
		language_menu->add_item(lang);
		if (lang == "GDScript") {
			default_language = i;
		}
	}
	if (default_language >= 0) {
		language_menu->select(default_language);
	}

	language_menu->connect(SceneStringName(item_selected), callable_mp(this, &ScriptCreateDialog::_language_changed));

	/* Inherits */

	base_type = "Object";

	HBoxContainer *hb = memnew(HBoxContainer);
	hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	parent_name = memnew(LineEdit);
	parent_name->connect(SceneStringName(text_changed), callable_mp(this, &ScriptCreateDialog::_parent_name_changed));
	parent_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hb->add_child(parent_name);
	register_text_enter(parent_name);
	parent_search_button = memnew(Button);
	parent_search_button->connect(SceneStringName(pressed), callable_mp(this, &ScriptCreateDialog::_browse_class_in_tree));
	hb->add_child(parent_search_button);
	parent_browse_button = memnew(Button);
	parent_browse_button->connect(SceneStringName(pressed), callable_mp(this, &ScriptCreateDialog::_browse_path).bind(true, false));
	hb->add_child(parent_browse_button);
	gc->add_child(memnew(Label(TTR("Inherits:"))));
	gc->add_child(hb);

	/* Templates */
	gc->add_child(memnew(Label(TTR("Template:"))));
	HBoxContainer *template_hb = memnew(HBoxContainer);
	template_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	use_templates = memnew(CheckBox);
	use_templates->set_pressed(is_using_templates);
	use_templates->connect(SceneStringName(pressed), callable_mp(this, &ScriptCreateDialog::_use_template_pressed));
	template_hb->add_child(use_templates);

	template_inactive_message = "";

	template_menu = memnew(OptionButton);
	template_menu->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	template_menu->connect(SceneStringName(item_selected), callable_mp(this, &ScriptCreateDialog::_template_changed));
	template_hb->add_child(template_menu);

	gc->add_child(template_hb);

	/* Built-in Script */

	built_in = memnew(CheckBox);
	built_in->set_text(TTR("On"));
	built_in->connect(SceneStringName(pressed), callable_mp(this, &ScriptCreateDialog::_built_in_pressed));
	gc->add_child(memnew(Label(TTR("Built-in Script:"))));
	gc->add_child(built_in);

	/* Path */

	hb = memnew(HBoxContainer);
	hb->connect(SceneStringName(sort_children), callable_mp(this, &ScriptCreateDialog::_path_hbox_sorted));
	file_path = memnew(LineEdit);
	file_path->connect(SceneStringName(text_changed), callable_mp(this, &ScriptCreateDialog::_path_changed));
	file_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hb->add_child(file_path);
	register_text_enter(file_path);
	path_button = memnew(Button);
	path_button->connect(SceneStringName(pressed), callable_mp(this, &ScriptCreateDialog::_browse_path).bind(false, true));
	hb->add_child(path_button);
	Label *label = memnew(Label(TTR("Path:")));
	gc->add_child(label);
	gc->add_child(hb);
	path_controls[0] = label;
	path_controls[1] = hb;

	/* Name */

	built_in_name = memnew(LineEdit);
	built_in_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	register_text_enter(built_in_name);
	label = memnew(Label(TTR("Name:")));
	gc->add_child(label);
	gc->add_child(built_in_name);
	name_controls[0] = label;
	name_controls[1] = built_in_name;
	label->hide();
	built_in_name->hide();

	/* Dialog Setup */

	select_class = memnew(CreateDialog);
	select_class->connect("create", callable_mp(this, &ScriptCreateDialog::_create));
	add_child(select_class);

	file_browse = memnew(EditorFileDialog);
	file_browse->connect("file_selected", callable_mp(this, &ScriptCreateDialog::_file_selected));
	file_browse->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	add_child(file_browse);
	set_ok_button_text(TTR("Create"));
	alert = memnew(AcceptDialog);
	alert->get_label()->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	alert->get_label()->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	alert->get_label()->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	alert->get_label()->set_custom_minimum_size(Size2(325, 60) * EDSCALE);
	add_child(alert);

	set_hide_on_ok(false);
	set_title(TTR("Attach Node Script"));
}
