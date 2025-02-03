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

#include "core/io/resource_saver.h"
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "core/string_builder.h"
#include "editor/create_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor_file_system.h"

void ScriptCreateDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				String lang = ScriptServer::get_language(i)->get_type();
				Ref<Texture> lang_icon = get_icon(lang, "EditorIcons");
				if (lang_icon.is_valid()) {
					language_menu->set_item_icon(i, lang_icon);
				}
			}

			String last_lang = EditorSettings::get_singleton()->get_project_metadata("script_setup", "last_selected_language", "");
			if (!last_lang.empty()) {
				for (int i = 0; i < language_menu->get_item_count(); i++) {
					if (language_menu->get_item_text(i) == last_lang) {
						language_menu->select(i);
						current_language = i;
						break;
					}
				}
			} else {
				language_menu->select(default_language);
			}

			path_button->set_icon(get_icon("Folder", "EditorIcons"));
			parent_browse_button->set_icon(get_icon("Folder", "EditorIcons"));
			parent_search_button->set_icon(get_icon("ClassList", "EditorIcons"));
			status_panel->add_style_override("panel", get_stylebox("bg", "Tree"));
		} break;
	}
}

void ScriptCreateDialog::_path_hbox_sorted() {
	if (is_visible()) {
		int filename_start_pos = initial_bp.rfind("/") + 1;
		int filename_end_pos = initial_bp.length();

		if (!is_built_in) {
			file_path->select(filename_start_pos, filename_end_pos);
		}

		// First set cursor to the end of line to scroll LineEdit view
		// to the right and then set the actual cursor position.
		file_path->set_cursor_position(file_path->get_text().length());
		file_path->set_cursor_position(filename_start_pos);

		file_path->grab_focus();
	}
}

bool ScriptCreateDialog::_can_be_built_in() {
	return (supports_built_in && built_in_enabled);
}

void ScriptCreateDialog::config(const String &p_base_name, const String &p_base_path, bool p_built_in_enabled, bool p_load_enabled) {
	class_name->set_text("");
	class_name->deselect();
	parent_name->set_text(p_base_name);
	parent_name->deselect();

	if (p_base_path != "") {
		initial_bp = p_base_path.get_basename();
		file_path->set_text(initial_bp + "." + ScriptServer::get_language(language_menu->get_selected())->get_extension());
		current_language = language_menu->get_selected();
	} else {
		initial_bp = "";
		file_path->set_text("");
	}
	file_path->deselect();

	built_in_enabled = p_built_in_enabled;
	load_enabled = p_load_enabled;

	_lang_changed(current_language);
	_class_name_changed("");
	_path_changed(file_path->get_text());
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

	return ClassDB::class_exists(p_string) || ScriptServer::is_global_class(p_string);
}

bool ScriptCreateDialog::_validate_class(const String &p_string) {
	if (p_string.length() == 0) {
		return false;
	}

	for (int i = 0; i < p_string.length(); i++) {
		if (i == 0) {
			if (p_string[0] >= '0' && p_string[0] <= '9') {
				return false; // no start with number plz
			}
		}

		bool valid_char = (p_string[i] >= '0' && p_string[i] <= '9') || (p_string[i] >= 'a' && p_string[i] <= 'z') || (p_string[i] >= 'A' && p_string[i] <= 'Z') || p_string[i] == '_' || p_string[i] == '.';

		if (!valid_char) {
			return false;
		}
	}

	return true;
}

String ScriptCreateDialog::_validate_path(const String &p_path, bool p_file_must_exist) {
	String p = p_path.strip_edges();

	if (p == "") {
		return TTR("Path is empty.");
	}
	if (p.get_file().get_basename() == "") {
		return TTR("Filename is empty.");
	}
	if (p.get_file().begins_with(".")) {
		return TTR("Name begins with a dot.");
	}

	p = ProjectSettings::get_singleton()->localize_path(p);
	if (!p.begins_with("res://")) {
		return TTR("Path is not local.");
	}

	DirAccess *d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (d->change_dir(p.get_base_dir()) != OK) {
		memdelete(d);
		return TTR("Invalid base path.");
	}
	memdelete(d);

	/* Does file already exist */
	DirAccess *f = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (f->dir_exists(p)) {
		memdelete(f);
		return TTR("A directory with the same name exists.");
	} else if (p_file_must_exist && !f->file_exists(p)) {
		memdelete(f);
		return TTR("File does not exist.");
	}
	memdelete(f);

	/* Check file extension */
	String extension = p.get_extension();
	List<String> extensions;

	// get all possible extensions for script
	for (int l = 0; l < language_menu->get_item_count(); l++) {
		ScriptServer::get_language(l)->get_recognized_extensions(&extensions);
	}

	bool found = false;
	bool match = false;
	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		if (E->get().nocasecmp_to(extension) == 0) {
			found = true;
			if (E->get() == ScriptServer::get_language(language_menu->get_selected())->get_extension()) {
				match = true;
			}
			break;
		}
	}

	if (!found) {
		return TTR("Invalid extension.");
	}
	if (!match) {
		return TTR("Wrong extension chosen.");
	}

	/* Let ScriptLanguage do custom validation */
	String path_error = ScriptServer::get_language(language_menu->get_selected())->validate_path(p);
	if (path_error != "") {
		return path_error;
	}

	/* All checks passed */
	return "";
}

String ScriptCreateDialog::_get_class_name() const {
	if (has_named_classes) {
		return class_name->get_text();
	} else {
		return ProjectSettings::get_singleton()->localize_path(file_path->get_text()).get_file().get_basename();
	}
}

void ScriptCreateDialog::_class_name_changed(const String &p_name) {
	if (_validate_class(class_name->get_text())) {
		is_class_name_valid = true;
	} else {
		is_class_name_valid = false;
	}
	_update_dialog();
}

void ScriptCreateDialog::_parent_name_changed(const String &p_parent) {
	if (_validate_parent(parent_name->get_text())) {
		is_parent_name_valid = true;
	} else {
		is_parent_name_valid = false;
	}
	_update_dialog();
}

void ScriptCreateDialog::_template_changed(int p_template) {
	String selected_template = p_template == 0 ? "" : template_menu->get_item_text(p_template);
	EditorSettings::get_singleton()->set_project_metadata("script_setup", "last_selected_template", selected_template);
	if (p_template == 0) {
		//default
		script_template = "";
		return;
	}
	int selected_id = template_menu->get_selected_id();

	for (int i = 0; i < template_list.size(); i++) {
		const ScriptTemplateInfo &sinfo = template_list[i];
		if (sinfo.id == selected_id) {
			script_template = sinfo.dir.plus_file(sinfo.name + "." + sinfo.extension);
			break;
		}
	}
}

void ScriptCreateDialog::ok_pressed() {
	if (is_new_script_created) {
		_create_new();
	} else {
		_load_exist();
	}

	is_new_script_created = true;
	_update_dialog();
}

void ScriptCreateDialog::_create_new() {
	String cname_param = _get_class_name();

	Ref<Script> scr;
	if (script_template != "") {
		scr = ResourceLoader::load(script_template);
		if (scr.is_null()) {
			alert->set_text(vformat(TTR("Error loading template '%s'"), script_template));
			alert->popup_centered();
			return;
		}
		scr = scr->duplicate();
		ScriptServer::get_language(language_menu->get_selected())->make_template(cname_param, parent_name->get_text(), scr);
	} else {
		scr = ScriptServer::get_language(language_menu->get_selected())->get_template(cname_param, parent_name->get_text());
	}

	if (has_named_classes) {
		String cname = class_name->get_text();
		if (cname.length()) {
			scr->set_name(cname);
		}
	}

	if (is_built_in) {
		scr->set_name(internal_name->get_text());
	} else {
		String lpath = ProjectSettings::get_singleton()->localize_path(file_path->get_text());
		scr->set_path(lpath);
		Error err = ResourceSaver::save(lpath, scr, ResourceSaver::FLAG_CHANGE_PATH);
		if (err != OK) {
			alert->set_text(TTR("Error - Could not create script in filesystem."));
			alert->popup_centered();
			return;
		}
	}

	emit_signal("script_created", scr);
	hide();
}

void ScriptCreateDialog::_load_exist() {
	String path = file_path->get_text();
	RES p_script = ResourceLoader::load(path, "Script");
	if (p_script.is_null()) {
		alert->set_text(vformat(TTR("Error loading script from %s"), path));
		alert->popup_centered();
		return;
	}

	emit_signal("script_created", p_script.get_ref_ptr());
	hide();
}

void ScriptCreateDialog::_lang_changed(int l) {
	ScriptLanguage *language = ScriptServer::get_language(l);

	has_named_classes = language->has_named_classes();
	can_inherit_from_file = language->can_inherit_from_file();
	supports_built_in = language->supports_builtin_mode();
	if (!supports_built_in) {
		is_built_in = false;
	}

	String selected_ext = "." + language->get_extension();
	String path = file_path->get_text();
	String extension = "";
	if (path != "") {
		if (path.find(".") != -1) {
			extension = path.get_extension();
		}

		if (extension.length() == 0) {
			// add extension if none
			path += selected_ext;
			_path_changed(path);
		} else {
			// change extension by selected language
			List<String> extensions;
			// get all possible extensions for script
			for (int m = 0; m < language_menu->get_item_count(); m++) {
				ScriptServer::get_language(m)->get_recognized_extensions(&extensions);
			}

			for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
				if (E->get().nocasecmp_to(extension) == 0) {
					path = path.get_basename() + selected_ext;
					_path_changed(path);
					break;
				}
			}
		}
	} else {
		path = "class" + selected_ext;
		_path_changed(path);
	}
	file_path->set_text(path);

	bool use_templates = language->is_using_templates();
	template_menu->set_disabled(!use_templates);
	template_menu->clear();

	if (use_templates) {
		_update_script_templates(language->get_extension());

		String last_lang = EditorSettings::get_singleton()->get_project_metadata("script_setup", "last_selected_language", "");
		String last_template = EditorSettings::get_singleton()->get_project_metadata("script_setup", "last_selected_template", "");

		template_menu->add_item(TTR("Default"));

		ScriptTemplateInfo *templates = template_list.ptrw();

		Vector<String> origin_names;
		origin_names.push_back(TTR("Project"));
		origin_names.push_back(TTR("Editor"));
		int cur_origin = -1;

		// Populate script template items previously sorted and now grouped by origin
		for (int i = 0; i < template_list.size(); i++) {
			if (int(templates[i].origin) != cur_origin) {
				template_menu->add_separator();

				String origin_name = origin_names[templates[i].origin];

				int last_index = template_menu->get_item_count() - 1;
				template_menu->set_item_text(last_index, origin_name);

				cur_origin = templates[i].origin;
			}
			String item_name = templates[i].name.capitalize();
			template_menu->add_item(item_name);

			int new_id = template_menu->get_item_count() - 1;
			templates[i].id = new_id;
		}
		// Disable overridden
		for (Map<String, Vector<int>>::Element *E = template_overrides.front(); E; E = E->next()) {
			const Vector<int> &overrides = E->get();

			if (overrides.size() == 1) {
				continue; // doesn't override anything
			}
			const ScriptTemplateInfo &extended = template_list[overrides[0]];

			StringBuilder override_info;
			override_info += TTR("Overrides");
			override_info += ": ";

			for (int i = 1; i < overrides.size(); i++) {
				const ScriptTemplateInfo &overridden = template_list[overrides[i]];

				int disable_index = template_menu->get_item_index(overridden.id);
				template_menu->set_item_disabled(disable_index, true);

				override_info += origin_names[overridden.origin];
				if (i < overrides.size() - 1) {
					override_info += ", ";
				}
			}
			template_menu->set_item_icon(extended.id, get_icon("Override", "EditorIcons"));
			template_menu->get_popup()->set_item_tooltip(extended.id, override_info.as_string());
		}
		// Reselect last selected template
		for (int i = 0; i < template_menu->get_item_count(); i++) {
			const String &ti = template_menu->get_item_text(i);
			if (language_menu->get_item_text(language_menu->get_selected()) == last_lang && last_template == ti) {
				template_menu->select(i);
				break;
			}
		}
	} else {
		template_menu->add_item(TTR("N/A"));
		script_template = "";
	}

	_template_changed(template_menu->get_selected());
	EditorSettings::get_singleton()->set_project_metadata("script_setup", "last_selected_language", language_menu->get_item_text(language_menu->get_selected()));

	_parent_name_changed(parent_name->get_text());
	_update_dialog();
}

void ScriptCreateDialog::_update_script_templates(const String &p_extension) {
	template_list.clear();
	template_overrides.clear();

	Vector<String> dirs;

	// Ordered from local to global for correct override mechanism
	dirs.push_back(EditorSettings::get_singleton()->get_project_script_templates_dir());
	dirs.push_back(EditorSettings::get_singleton()->get_script_templates_dir());

	for (int i = 0; i < dirs.size(); i++) {
		Vector<String> list = EditorSettings::get_singleton()->get_script_templates(p_extension, dirs[i]);

		for (int j = 0; j < list.size(); j++) {
			ScriptTemplateInfo sinfo;
			sinfo.origin = ScriptOrigin(i);
			sinfo.dir = dirs[i];
			sinfo.name = list[j];
			sinfo.extension = p_extension;
			template_list.push_back(sinfo);

			if (!template_overrides.has(sinfo.name)) {
				Vector<int> overrides;
				overrides.push_back(template_list.size() - 1); // first one
				template_overrides.insert(sinfo.name, overrides);
			} else {
				Vector<int> &overrides = template_overrides[sinfo.name];
				overrides.push_back(template_list.size() - 1);
			}
		}
	}
}

void ScriptCreateDialog::_built_in_pressed() {
	if (internal->is_pressed()) {
		is_built_in = true;
		is_new_script_created = true;
	} else {
		is_built_in = false;
		_path_changed(file_path->get_text());
	}
	_update_dialog();
}

void ScriptCreateDialog::_browse_path(bool browse_parent, bool p_save) {
	is_browsing_parent = browse_parent;

	if (p_save) {
		file_browse->set_mode(EditorFileDialog::MODE_SAVE_FILE);
		file_browse->set_title(TTR("Open Script / Choose Location"));
		file_browse->get_ok()->set_text(TTR("Open"));
	} else {
		file_browse->set_mode(EditorFileDialog::MODE_OPEN_FILE);
		file_browse->set_title(TTR("Open Script"));
	}

	file_browse->set_disable_overwrite_warning(true);
	file_browse->clear_filters();
	List<String> extensions;

	int lang = language_menu->get_selected();
	ScriptServer::get_language(lang)->get_recognized_extensions(&extensions);

	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		file_browse->add_filter("*." + E->get());
	}

	file_browse->set_current_path(file_path->get_text());
	file_browse->popup_centered_ratio();
}

void ScriptCreateDialog::_file_selected(const String &p_file) {
	String p = ProjectSettings::get_singleton()->localize_path(p_file);
	if (is_browsing_parent) {
		parent_name->set_text("\"" + p + "\"");
		_parent_name_changed(parent_name->get_text());
	} else {
		file_path->set_text(p);
		_path_changed(p);

		String filename = p.get_file().get_basename();
		int select_start = p.rfind(filename);
		file_path->select(select_start, select_start + filename.length());
		file_path->set_cursor_position(select_start + filename.length());
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
}

void ScriptCreateDialog::_path_changed(const String &p_path) {
	if (is_built_in) {
		return;
	}

	is_path_valid = false;
	is_new_script_created = true;

	String path_error = _validate_path(p_path, false);
	if (path_error != "") {
		_msg_path_valid(false, path_error);
		_update_dialog();
		return;
	}

	/* Does file already exist */
	DirAccess *f = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	String p = ProjectSettings::get_singleton()->localize_path(p_path.strip_edges());
	if (f->file_exists(p)) {
		is_new_script_created = false;
		_msg_path_valid(true, TTR("File exists, it will be reused."));
	}
	memdelete(f);

	is_path_valid = true;
	_update_dialog();
}

void ScriptCreateDialog::_path_entered(const String &p_path) {
	ok_pressed();
}

void ScriptCreateDialog::_msg_script_valid(bool valid, const String &p_msg) {
	error_label->set_text(String::utf8("• ") + TTR(p_msg));
	if (valid) {
		error_label->add_color_override("font_color", get_color("success_color", "Editor"));
	} else {
		error_label->add_color_override("font_color", get_color("error_color", "Editor"));
	}
}

void ScriptCreateDialog::_msg_path_valid(bool valid, const String &p_msg) {
	path_error_label->set_text(String::utf8("• ") + TTR(p_msg));
	if (valid) {
		path_error_label->add_color_override("font_color", get_color("success_color", "Editor"));
	} else {
		path_error_label->add_color_override("font_color", get_color("error_color", "Editor"));
	}
}

void ScriptCreateDialog::_update_dialog() {
	/* "Add Script Dialog" GUI logic and script checks. */

	bool script_ok = true;

	// Is script path/name valid (order from top to bottom)?

	if (!is_built_in && !is_path_valid) {
		_msg_script_valid(false, TTR("Invalid path."));
		script_ok = false;
	}
	if (has_named_classes && (is_new_script_created && !is_class_name_valid)) {
		_msg_script_valid(false, TTR("Invalid class name."));
		script_ok = false;
	}
	if (!is_parent_name_valid && is_new_script_created) {
		_msg_script_valid(false, TTR("Invalid inherited parent name or path."));
		script_ok = false;
	}

	if (script_ok) {
		_msg_script_valid(true, TTR("Script path/name is valid."));
	}

	// Does script have named classes?

	if (has_named_classes) {
		if (is_new_script_created) {
			class_name->set_editable(true);
			class_name->set_placeholder(TTR("Allowed: a-z, A-Z, 0-9, _ and ."));
			class_name->set_placeholder_alpha(0.3);
		} else {
			class_name->set_editable(false);
		}
	} else {
		class_name->set_editable(false);
		class_name->set_placeholder(TTR("N/A"));
		class_name->set_placeholder_alpha(1);
		class_name->set_text("");
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
	internal->set_text(is_built_in ? TTR("On") : TTR("Off"));

	if (!_can_be_built_in()) {
		internal->set_pressed(false);
	}
	internal->set_disabled(!_can_be_built_in());

	// Is Script created or loaded from existing file?

	builtin_warning_label->set_visible(is_built_in);

	path_controls[0]->set_visible(!is_built_in);
	path_controls[1]->set_visible(!is_built_in);
	name_controls[0]->set_visible(is_built_in);
	name_controls[1]->set_visible(is_built_in);

	// Check if the script name is the same as the parent class.
	// This warning isn't relevant if the script is built-in.
	script_name_warning_label->set_visible(!is_built_in && _get_class_name() == parent_name->get_text());

	if (is_built_in) {
		get_ok()->set_text(TTR("Create"));
		parent_name->set_editable(true);
		parent_search_button->set_disabled(false);
		parent_browse_button->set_disabled(!can_inherit_from_file);
		_msg_path_valid(true, TTR("Built-in script (into scene file)."));
	} else if (is_new_script_created) {
		// New script created.

		get_ok()->set_text(TTR("Create"));
		parent_name->set_editable(true);
		parent_search_button->set_disabled(false);
		parent_browse_button->set_disabled(!can_inherit_from_file);
		if (is_path_valid) {
			_msg_path_valid(true, TTR("Will create a new script file."));
		}
	} else if (load_enabled) {
		// Script loaded.

		get_ok()->set_text(TTR("Load"));
		parent_name->set_editable(false);
		parent_search_button->set_disabled(true);
		parent_browse_button->set_disabled(true);
		if (is_path_valid) {
			_msg_path_valid(true, TTR("Will load an existing script file."));
		}
	} else {
		get_ok()->set_text(TTR("Create"));
		parent_name->set_editable(true);
		parent_search_button->set_disabled(false);
		parent_browse_button->set_disabled(!can_inherit_from_file);
		_msg_path_valid(false, TTR("Script file already exists."));

		script_ok = false;
	}

	get_ok()->set_disabled(!script_ok);
	set_size(Vector2());
	minimum_size_changed();
}

void ScriptCreateDialog::_bind_methods() {
	ClassDB::bind_method("_path_hbox_sorted", &ScriptCreateDialog::_path_hbox_sorted);
	ClassDB::bind_method("_class_name_changed", &ScriptCreateDialog::_class_name_changed);
	ClassDB::bind_method("_parent_name_changed", &ScriptCreateDialog::_parent_name_changed);
	ClassDB::bind_method("_lang_changed", &ScriptCreateDialog::_lang_changed);
	ClassDB::bind_method("_built_in_pressed", &ScriptCreateDialog::_built_in_pressed);
	ClassDB::bind_method("_browse_path", &ScriptCreateDialog::_browse_path);
	ClassDB::bind_method("_file_selected", &ScriptCreateDialog::_file_selected);
	ClassDB::bind_method("_path_changed", &ScriptCreateDialog::_path_changed);
	ClassDB::bind_method("_path_entered", &ScriptCreateDialog::_path_entered);
	ClassDB::bind_method("_template_changed", &ScriptCreateDialog::_template_changed);
	ClassDB::bind_method("_create", &ScriptCreateDialog::_create);
	ClassDB::bind_method("_browse_class_in_tree", &ScriptCreateDialog::_browse_class_in_tree);

	ClassDB::bind_method(D_METHOD("config", "inherits", "path", "built_in_enabled", "load_enabled"), &ScriptCreateDialog::config, DEFVAL(true), DEFVAL(true));

	ADD_SIGNAL(MethodInfo("script_created", PropertyInfo(Variant::OBJECT, "script", PROPERTY_HINT_RESOURCE_TYPE, "Script")));
}

ScriptCreateDialog::ScriptCreateDialog() {
	/* DIALOG */

	/* Main Controls */

	GridContainer *gc = memnew(GridContainer);
	gc->set_columns(2);

	/* Error Messages Field */

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_custom_minimum_size(Size2(340, 30) * EDSCALE);

	error_label = memnew(Label);
	vb->add_child(error_label);

	path_error_label = memnew(Label);
	vb->add_child(path_error_label);

	builtin_warning_label = memnew(Label);
	builtin_warning_label->set_custom_minimum_size(Size2(340, 10) * EDSCALE);
	builtin_warning_label->set_text(
			TTR("Note: Built-in scripts have some limitations and can't be edited using an external editor."));
	vb->add_child(builtin_warning_label);
	builtin_warning_label->set_autowrap(true);
	builtin_warning_label->hide();

	script_name_warning_label = memnew(Label);
	script_name_warning_label->set_custom_minimum_size(Size2(340, 10) * EDSCALE);
	script_name_warning_label->set_text(
			TTR("Warning: Having the script name be the same as a built-in type is usually not desired."));
	vb->add_child(script_name_warning_label);
	script_name_warning_label->add_color_override("font_color", Color(1, 0.85, 0.4));
	script_name_warning_label->set_autowrap(true);
	script_name_warning_label->hide();

	status_panel = memnew(PanelContainer);
	status_panel->set_custom_minimum_size(Size2(350, 40) * EDSCALE);
	status_panel->set_h_size_flags(Control::SIZE_FILL);
	status_panel->add_child(vb);

	/* Spacing */

	Control *spacing = memnew(Control);
	spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

	vb = memnew(VBoxContainer);
	vb->add_child(gc);
	vb->add_child(spacing);
	vb->add_child(status_panel);
	HBoxContainer *hb = memnew(HBoxContainer);
	hb->add_child(vb);

	add_child(hb);

	/* Language */

	language_menu = memnew(OptionButton);
	language_menu->set_custom_minimum_size(Size2(250, 0) * EDSCALE);
	language_menu->set_h_size_flags(SIZE_EXPAND_FILL);
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
	current_language = default_language;

	language_menu->connect("item_selected", this, "_lang_changed");

	/* Inherits */

	base_type = "Object";

	hb = memnew(HBoxContainer);
	hb->set_h_size_flags(SIZE_EXPAND_FILL);
	parent_name = memnew(LineEdit);
	parent_name->connect("text_changed", this, "_parent_name_changed");
	parent_name->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(parent_name);
	parent_search_button = memnew(Button);
	parent_search_button->set_flat(true);
	parent_search_button->connect("pressed", this, "_browse_class_in_tree");
	hb->add_child(parent_search_button);
	parent_browse_button = memnew(Button);
	parent_browse_button->set_flat(true);
	parent_browse_button->connect("pressed", this, "_browse_path", varray(true, false));
	hb->add_child(parent_browse_button);
	gc->add_child(memnew(Label(TTR("Inherits:"))));
	gc->add_child(hb);
	is_browsing_parent = false;

	/* Class Name */

	class_name = memnew(LineEdit);
	class_name->connect("text_changed", this, "_class_name_changed");
	class_name->set_h_size_flags(SIZE_EXPAND_FILL);
	gc->add_child(memnew(Label(TTR("Class Name:"))));
	gc->add_child(class_name);

	/* Templates */

	template_menu = memnew(OptionButton);
	gc->add_child(memnew(Label(TTR("Template:"))));
	gc->add_child(template_menu);
	template_menu->connect("item_selected", this, "_template_changed");

	/* Built-in Script */

	internal = memnew(CheckBox);
	internal->set_text(TTR("Off"));
	internal->connect("pressed", this, "_built_in_pressed");
	gc->add_child(memnew(Label(TTR("Built-in Script:"))));
	gc->add_child(internal);

	/* Path */

	hb = memnew(HBoxContainer);
	hb->connect("sort_children", this, "_path_hbox_sorted");
	file_path = memnew(LineEdit);
	file_path->connect("text_changed", this, "_path_changed");
	file_path->connect("text_entered", this, "_path_entered");
	file_path->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(file_path);
	path_button = memnew(Button);
	path_button->set_flat(true);
	path_button->connect("pressed", this, "_browse_path", varray(false, true));
	hb->add_child(path_button);
	Label *label = memnew(Label(TTR("Path:")));
	gc->add_child(label);
	gc->add_child(hb);
	re_check_path = false;
	path_controls[0] = label;
	path_controls[1] = hb;

	/* Name */

	internal_name = memnew(LineEdit);
	internal_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label = memnew(Label(TTR("Name:")));
	gc->add_child(label);
	gc->add_child(internal_name);
	name_controls[0] = label;
	name_controls[1] = internal_name;
	label->hide();
	internal_name->hide();

	/* Dialog Setup */

	select_class = memnew(CreateDialog);
	select_class->connect("create", this, "_create");
	add_child(select_class);

	file_browse = memnew(EditorFileDialog);
	file_browse->connect("file_selected", this, "_file_selected");
	file_browse->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	add_child(file_browse);
	get_ok()->set_text(TTR("Create"));
	alert = memnew(AcceptDialog);
	alert->set_as_minsize();
	alert->get_label()->set_autowrap(true);
	alert->get_label()->set_align(Label::ALIGN_CENTER);
	alert->get_label()->set_valign(Label::VALIGN_CENTER);
	alert->get_label()->set_custom_minimum_size(Size2(325, 60) * EDSCALE);
	add_child(alert);

	set_as_minsize();
	set_hide_on_ok(false);
	set_title(TTR("Attach Node Script"));

	is_parent_name_valid = false;
	is_class_name_valid = false;
	is_path_valid = false;

	has_named_classes = false;
	supports_built_in = false;
	can_inherit_from_file = false;
	is_built_in = false;
	built_in_enabled = true;
	load_enabled = true;

	is_new_script_created = true;
}
