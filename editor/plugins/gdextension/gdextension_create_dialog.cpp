/**************************************************************************/
/*  gdextension_create_dialog.cpp                                         */
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

#include "gdextension_create_dialog.h"

#include "gdextension_creator_plugin.h"

#include "core/io/dir_access.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"

void GDExtensionCreateDialog::load_plugin_creators(const Vector<Ref<GDExtensionCreatorPlugin>> &p_plugin_creators) {
	plugin_creators = p_plugin_creators;
	language_option->clear();
	language_option_index_map.clear();
	for (int i = 0; i < plugin_creators.size(); i++) {
		const Ref<GDExtensionCreatorPlugin> plugin_creator = plugin_creators[i];
		plugin_creator->setup_creator();
		const Vector<String> lang_variations = plugin_creator->get_language_variations();
		for (int j = 0; j < lang_variations.size(); j++) {
			language_option->add_item(lang_variations[j]);
			language_option_index_map.push_back(Vector2i(i, j));
		}
	}
	language_option->select(0);
}

void GDExtensionCreateDialog::_clear_fields() {
	base_name_edit->clear();
	library_name_edit->clear();
	path_edit->clear();
	library_name_edit->set_placeholder("my_extension");
	path_edit->set_placeholder("res://addons/my_extension");
}

void GDExtensionCreateDialog::_on_canceled() {
	_clear_fields();
}

void GDExtensionCreateDialog::_on_confirmed() {
	const String valid_base_name = _get_valid_base_name();
	const String valid_library_name = _get_valid_library_name(valid_base_name);
	const String valid_path = _get_valid_path(valid_base_name);
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	dir->make_dir_recursive(valid_path);
	Vector2i selected_indices = language_option_index_map[language_option->get_selected_id()];
	Ref<GDExtensionCreatorPlugin> plugin_creator = plugin_creators[selected_indices.x];
	plugin_creator->create_gdextension(valid_path, valid_base_name, valid_library_name, selected_indices.y, compile_checkbox->is_pressed());
	_clear_fields();
	emit_signal("gdextension_created");
}

void GDExtensionCreateDialog::_on_required_text_changed() {
	// Erase existing messages.
	const int start_message_count = validation_panel->get_message_count();
	for (int i = 1; i < start_message_count; i++) {
		validation_panel->set_message(i, "", EditorValidationPanel::MSG_INFO);
	}
	if (base_name_edit->get_text().is_empty()) {
		validation_panel->set_message(MSG_ID_BASE_NAME, TTR("Please specify a base name."), EditorValidationPanel::MSG_ERROR);
		library_name_edit->set_placeholder("my_extension");
		path_edit->set_placeholder("res://addons/my_extension");
		return;
	}
	// Update included messages for the text boxes.
	const String valid_base_name = _get_valid_base_name();
	const String valid_library_name = _get_valid_library_name(valid_base_name);
	const String valid_path = _get_valid_path(valid_base_name);
	library_name_edit->set_placeholder(valid_library_name);
	path_edit->set_placeholder(valid_path);
	validation_panel->set_message(MSG_ID_BASE_NAME, TTR("Base name will be: ") + valid_base_name, EditorValidationPanel::MSG_OK);
	validation_panel->set_message(MSG_ID_LIBRARY_NAME, TTR("Library name will be: ") + valid_library_name, EditorValidationPanel::MSG_OK);
	validation_panel->set_message(MSG_ID_PATH, TTR("Path will be: ") + valid_path, EditorValidationPanel::MSG_OK);
	// Write new custom messages.
	Vector2i selected_indices = language_option_index_map[language_option->get_selected_id()];
	Ref<GDExtensionCreatorPlugin> plugin_creator = plugin_creators[selected_indices.x];
	Dictionary validation_messages = plugin_creator->get_validation_messages(valid_base_name, valid_library_name, valid_path, selected_indices.y, compile_checkbox->is_pressed());
	Array messages = validation_messages.keys();
	for (int i = 0; i < messages.size(); i++) {
		const String message = messages[i];
		const int status = validation_messages[message];
		const int index = i + MSG_ID_MAX;
		if (index >= start_message_count) {
			validation_panel->add_line(index, "");
		}
		validation_panel->set_message(index, message, (EditorValidationPanel::MessageType)status);
	}
}

String GDExtensionCreateDialog::_get_valid_base_name() {
	String text = base_name_edit->get_text().strip_edges().validate_ascii_identifier();
	if (text.begins_with("_")) {
		return text.substr(1);
	}
	return text;
}

String GDExtensionCreateDialog::_get_valid_library_name(const String &p_valid_base_name) {
	String text = library_name_edit->get_text().strip_edges();
	if (text.is_empty()) {
		text = p_valid_base_name;
	}
	if (is_digit(text[0])) {
		text = "godot_" + text;
	}
	return text.validate_ascii_identifier();
}

String GDExtensionCreateDialog::_get_valid_path(const String &p_valid_base_name) {
	String text = path_edit->get_text().strip_edges();
	if (text.is_empty()) {
		text = p_valid_base_name;
	}
	if (!text.begins_with("res://")) {
		if (text.contains("/")) {
			return "res://" + text;
		} else {
			return "res://addons/" + text;
		}
	}
	return text;
}

void GDExtensionCreateDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("gdextension_created"));
}

void GDExtensionCreateDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				base_name_edit->grab_focus();
			}
		} break;

		case NOTIFICATION_READY: {
			connect(SceneStringName(confirmed), callable_mp(this, &GDExtensionCreateDialog::_on_confirmed));
			get_cancel_button()->connect(SceneStringName(pressed), callable_mp(this, &GDExtensionCreateDialog::_on_canceled));
		} break;
	}
}

GDExtensionCreateDialog::GDExtensionCreateDialog() {
	get_ok_button()->set_disabled(true);
	get_ok_button()->set_text(TTR("Create"));
	set_hide_on_ok(true);
	set_title(TTR("Create GDExtension"));

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(vbox);

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	grid->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->add_child(grid);

	// Base name line edit.
	Label *base_name_label = memnew(Label);
	base_name_label->set_text(TTR("Base Name:"));
	base_name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(base_name_label);

	base_name_edit = memnew(LineEdit);
	base_name_edit->set_placeholder("my_extension");
	base_name_edit->set_tooltip_text(TTR("Required. The base name of the extension. Must be valid as an identifier continuation in a programming language.\nThis will be used to determine the library and folder names, if unspecified.\nFor engine modules, this must match the module's folder name."));
	base_name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(base_name_edit);

	// Library name line edit.
	Label *library_name_label = memnew(Label);
	library_name_label->set_text(TTR("Library Name:"));
	library_name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(library_name_label);

	library_name_edit = memnew(LineEdit);
	library_name_edit->set_placeholder("my_extension");
	library_name_edit->set_tooltip_text(TTR("This should be similar to the base name, but must on its own be a valid identifier in a programming language.\nAs a good practice, this should be a superset of the base name."));
	library_name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(library_name_edit);

	// Path line edit.
	Label *path_label = memnew(Label);
	path_label->set_text(TTR("Path:"));
	path_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(path_label);

	path_edit = memnew(LineEdit);
	path_edit->set_placeholder("res://addons/my_extension");
	path_edit->set_tooltip_text(TTR("The path to the extension (relative to the project root). If you type this manually, res:// will be added for you."));
	path_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(path_edit);

	// Language dropdown.
	Label *language_label = memnew(Label);
	language_label->set_text(TTR("Language:"));
	language_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(language_label);

	language_option = memnew(OptionButton);
	language_option->set_tooltip_text(TTR("The language to use for the extension."));
	grid->add_child(language_option);

	// Compile now checkbox.
	Label *compile_label = memnew(Label);
	compile_label->set_text(TTR("Compile now?"));
	compile_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(compile_label);

	compile_checkbox = memnew(CheckBox);
	compile_checkbox->set_pressed(true);
	grid->add_child(compile_checkbox);

	Control *spacing = memnew(Control);
	vbox->add_child(spacing);
	spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

	validation_panel = memnew(EditorValidationPanel);
	validation_panel->set_custom_minimum_size(Size2(500, 200) * EDSCALE);
	validation_panel->add_line(MSG_ID_BASE_NAME, TTR("Please specify a base name."));
	validation_panel->add_line(MSG_ID_LIBRARY_NAME, TTR("Please specify a base name."));
	validation_panel->add_line(MSG_ID_PATH, TTR("Please specify a base name."));
	validation_panel->set_update_callback(callable_mp(this, &GDExtensionCreateDialog::_on_required_text_changed));
	validation_panel->set_accept_button(get_ok_button());
	vbox->add_child(validation_panel);
	validation_panel->update();

	base_name_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	library_name_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	path_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	language_option->connect(SceneStringName(item_selected), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	compile_checkbox->connect(SceneStringName(pressed), callable_mp(validation_panel, &EditorValidationPanel::update));
}
