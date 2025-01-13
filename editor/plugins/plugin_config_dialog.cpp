/**************************************************************************/
/*  plugin_config_dialog.cpp                                              */
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

#include "plugin_config_dialog.h"

#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/object/script_language.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/project_settings_editor.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/grid_container.h"

void PluginConfigDialog::_clear_fields() {
	name_edit->set_text("");
	subfolder_edit->set_text("");
	desc_edit->set_text("");
	author_edit->set_text("");
	version_edit->set_text("");
	script_edit->set_text("");
}

void PluginConfigDialog::_on_confirmed() {
	String path = "res://addons/" + _get_subfolder();

	if (!_edit_mode) {
		Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (d.is_null() || d->make_dir_recursive(path) != OK) {
			return;
		}
	}
	// Create the plugin.cfg file.
	Ref<ConfigFile> cf = memnew(ConfigFile);
	cf->load(path.path_join("plugin.cfg"));
	cf->set_value("plugin", "name", name_edit->get_text());
	cf->set_value("plugin", "description", desc_edit->get_text());
	cf->set_value("plugin", "author", author_edit->get_text());
	cf->set_value("plugin", "version", version_edit->get_text());
	// Language-specific settings.
	int lang_index = script_option_edit->get_selected();
	_create_script_for_plugin(path, cf, lang_index);
	// Save and inform the editor.
	cf->save(path.path_join("plugin.cfg"));
	EditorNode::get_singleton()->get_project_settings()->update_plugins();
	EditorFileSystem::get_singleton()->scan();
	_clear_fields();
}

void PluginConfigDialog::_create_script_for_plugin(const String &p_plugin_path, Ref<ConfigFile> p_config_file, int p_script_lang_index) {
	ScriptLanguage *language = ScriptServer::get_language(p_script_lang_index);
	ERR_FAIL_COND(language == nullptr);
	String ext = language->get_extension();
	String script_name = script_edit->get_text().is_empty() ? _get_subfolder() : script_edit->get_text();
	if (script_name.get_extension() != ext) {
		script_name += "." + ext;
	}
	String script_path = p_plugin_path.path_join(script_name);
	p_config_file->set_value("plugin", "script", script_name);
	// If the requested script does not exist, create it.
	if (!_edit_mode && !FileAccess::exists(script_path)) {
		String class_name = script_name.get_basename();
		String template_content = "";
		Vector<ScriptLanguage::ScriptTemplate> templates = language->get_built_in_templates("EditorPlugin");
		if (!templates.is_empty()) {
			template_content = templates[0].content;
		}
		Ref<Script> scr = language->make_template(template_content, class_name, "EditorPlugin");
		scr->set_path(script_path, true);
		ResourceSaver::save(scr);
		p_config_file->save(p_plugin_path.path_join("plugin.cfg"));
		emit_signal(SNAME("plugin_ready"), scr.ptr(), active_edit->is_pressed() ? _to_absolute_plugin_path(_get_subfolder()) : "");
	}
}

void PluginConfigDialog::_on_canceled() {
	_clear_fields();
}

void PluginConfigDialog::_on_required_text_changed() {
	if (name_edit->get_text().is_empty()) {
		validation_panel->set_message(MSG_ID_PLUGIN, TTR("Plugin name cannot be blank."), EditorValidationPanel::MSG_ERROR);
	}
	if (subfolder_edit->is_visible()) {
		if (!subfolder_edit->get_text().is_empty() && !subfolder_edit->get_text().is_valid_filename()) {
			validation_panel->set_message(MSG_ID_SUBFOLDER, TTR("Subfolder name is not a valid folder name."), EditorValidationPanel::MSG_ERROR);
		} else {
			String path = "res://addons/" + _get_subfolder();
			if (!_edit_mode && DirAccess::exists(path)) { // Only show this error if in "create" mode.
				validation_panel->set_message(MSG_ID_SUBFOLDER, TTR("Subfolder cannot be one which already exists."), EditorValidationPanel::MSG_ERROR);
			}
		}
	} else {
		validation_panel->set_message(MSG_ID_SUBFOLDER, "", EditorValidationPanel::MSG_OK);
	}
	// Language and script validation.
	int lang_idx = script_option_edit->get_selected();
	ScriptLanguage *language = ScriptServer::get_language(lang_idx);
	if (language == nullptr) {
		return;
	}
	String ext = language->get_extension();
	if ((!script_edit->get_text().get_extension().is_empty() && script_edit->get_text().get_extension() != ext) || script_edit->get_text().ends_with(".")) {
		validation_panel->set_message(MSG_ID_SCRIPT, vformat(TTR("Script extension must match chosen language extension (.%s)."), ext), EditorValidationPanel::MSG_ERROR);
	}
	if (active_edit->is_visible()) {
		if (language->get_name() == "C#") {
			active_edit->set_pressed(false);
			active_edit->set_disabled(true);
			validation_panel->set_message(MSG_ID_ACTIVE, TTR("C# doesn't support activating the plugin on creation because the project must be built first."), EditorValidationPanel::MSG_WARNING);
		} else {
			active_edit->set_disabled(false);
		}
	}
}

String PluginConfigDialog::_get_subfolder() {
	return subfolder_edit->get_text().is_empty() ? name_edit->get_text().replace(" ", "_").to_lower() : subfolder_edit->get_text();
}

String PluginConfigDialog::_to_absolute_plugin_path(const String &p_plugin_name) {
	return "res://addons/" + p_plugin_name + "/plugin.cfg";
}

void PluginConfigDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				name_edit->grab_focus();
			}
		} break;

		case NOTIFICATION_READY: {
			connect(SceneStringName(confirmed), callable_mp(this, &PluginConfigDialog::_on_confirmed));
			get_cancel_button()->connect(SceneStringName(pressed), callable_mp(this, &PluginConfigDialog::_on_canceled));
		} break;
	}
}

void PluginConfigDialog::config(const String &p_config_path) {
	if (!p_config_path.is_empty()) {
		Ref<ConfigFile> cf = memnew(ConfigFile);
		Error err = cf->load(p_config_path);
		ERR_FAIL_COND_MSG(err != OK, "Cannot load config file from path '" + p_config_path + "'.");

		name_edit->set_text(cf->get_value("plugin", "name", ""));
		subfolder_edit->set_text(p_config_path.get_base_dir().get_file());
		desc_edit->set_text(cf->get_value("plugin", "description", ""));
		author_edit->set_text(cf->get_value("plugin", "author", ""));
		version_edit->set_text(cf->get_value("plugin", "version", ""));
		script_edit->set_text(cf->get_value("plugin", "script", ""));

		_edit_mode = true;
		set_title(TTR("Edit a Plugin"));
	} else {
		_clear_fields();
		_edit_mode = false;
		set_title(TTR("Create a Plugin"));
	}

	for (Control *control : plugin_edit_hidden_controls) {
		control->set_visible(!_edit_mode);
	}

	validation_panel->update();

	get_ok_button()->set_disabled(!_edit_mode);
	set_ok_button_text(_edit_mode ? TTR("Update") : TTR("Create"));
}

void PluginConfigDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("plugin_ready", PropertyInfo(Variant::STRING, "script_path", PROPERTY_HINT_NONE, ""), PropertyInfo(Variant::STRING, "activate_name")));
}

PluginConfigDialog::PluginConfigDialog() {
	get_ok_button()->set_disabled(true);
	set_hide_on_ok(true);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(vbox);

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	grid->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->add_child(grid);

	// Plugin Name
	Label *name_lb = memnew(Label);
	name_lb->set_text(TTR("Plugin Name:"));
	name_lb->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(name_lb);

	name_edit = memnew(LineEdit);
	name_edit->set_placeholder("MyPlugin");
	name_edit->set_tooltip_text(TTR("Required. This name will be displayed in the list of plugins."));
	name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(name_edit);

	// Subfolder
	Label *subfolder_lb = memnew(Label);
	subfolder_lb->set_text(TTR("Subfolder:"));
	subfolder_lb->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(subfolder_lb);
	plugin_edit_hidden_controls.push_back(subfolder_lb);

	subfolder_edit = memnew(LineEdit);
	subfolder_edit->set_placeholder("\"my_plugin\" -> res://addons/my_plugin");
	subfolder_edit->set_tooltip_text(TTR("Optional. The folder name should generally use `snake_case` naming (avoid spaces and special characters).\nIf left empty, the folder will be named after the plugin name converted to `snake_case`."));
	subfolder_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(subfolder_edit);
	plugin_edit_hidden_controls.push_back(subfolder_edit);

	// Description
	Label *desc_lb = memnew(Label);
	desc_lb->set_text(TTR("Description:"));
	desc_lb->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(desc_lb);

	desc_edit = memnew(TextEdit);
	desc_edit->set_tooltip_text(TTR("Optional. This description should be kept relatively short (up to 5 lines).\nIt will display when hovering the plugin in the list of plugins."));
	desc_edit->set_custom_minimum_size(Size2(400, 80) * EDSCALE);
	desc_edit->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	desc_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	desc_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(desc_edit);

	// Author
	Label *author_lb = memnew(Label);
	author_lb->set_text(TTR("Author:"));
	author_lb->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(author_lb);

	author_edit = memnew(LineEdit);
	author_edit->set_placeholder("Godette");
	author_edit->set_tooltip_text(TTR("Optional. The author's username, full name, or organization name."));
	author_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(author_edit);

	// Version
	Label *version_lb = memnew(Label);
	version_lb->set_text(TTR("Version:"));
	version_lb->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(version_lb);

	version_edit = memnew(LineEdit);
	version_edit->set_tooltip_text(TTR("Optional. A human-readable version identifier used for informational purposes only."));
	version_edit->set_placeholder("1.0");
	version_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(version_edit);

	// Language dropdown
	Label *script_option_lb = memnew(Label);
	script_option_lb->set_text(TTR("Language:"));
	script_option_lb->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(script_option_lb);

	script_option_edit = memnew(OptionButton);
	script_option_edit->set_tooltip_text(TTR("Required. The scripting language to use for the script.\nNote that a plugin may use several languages at once by adding more scripts to the plugin."));
	int default_lang = 0;
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		ScriptLanguage *lang = ScriptServer::get_language(i);
		script_option_edit->add_item(lang->get_name());
		if (lang->get_name() == "GDScript") {
			default_lang = i;
		}
	}
	script_option_edit->select(default_lang);
	grid->add_child(script_option_edit);

	// Plugin Script Name
	Label *script_name_label = memnew(Label);
	script_name_label->set_text(TTR("Script Name:"));
	script_name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(script_name_label);

	script_edit = memnew(LineEdit);
	script_edit->set_tooltip_text(TTR("Optional. The name of the script file. If left empty, will default to the subfolder name."));
	script_edit->set_placeholder("\"plugin.gd\" -> res://addons/my_plugin/plugin.gd");
	script_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(script_edit);

	// Activate now checkbox
	Label *active_label = memnew(Label);
	active_label->set_text(TTR("Activate now?"));
	active_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(active_label);
	plugin_edit_hidden_controls.push_back(active_label);

	active_edit = memnew(CheckBox);
	active_edit->set_pressed(true);
	grid->add_child(active_edit);
	plugin_edit_hidden_controls.push_back(active_edit);

	Control *spacing = memnew(Control);
	vbox->add_child(spacing);
	spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

	validation_panel = memnew(EditorValidationPanel);
	vbox->add_child(validation_panel);
	validation_panel->add_line(MSG_ID_PLUGIN, TTR("Plugin name is valid."));
	validation_panel->add_line(MSG_ID_SCRIPT, TTR("Script extension is valid."));
	validation_panel->add_line(MSG_ID_SUBFOLDER, TTR("Subfolder name is valid."));
	validation_panel->add_line(MSG_ID_ACTIVE, "");
	validation_panel->set_update_callback(callable_mp(this, &PluginConfigDialog::_on_required_text_changed));
	validation_panel->set_accept_button(get_ok_button());

	script_option_edit->connect(SceneStringName(item_selected), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	name_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	subfolder_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	script_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
}

PluginConfigDialog::~PluginConfigDialog() {
}
