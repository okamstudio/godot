/**************************************************************************/
/*  gdextension_edit_dialog.cpp                                           */
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

#include "gdextension_edit_dialog.h"

#include "core/extension/gdextension_manager.h"
#include "core/version.h"
#include "editor/editor_properties_array_dict.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"

void GDExtensionEditDialog::load_gdextension_config(const String &p_path) {
	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(p_path);
	ERR_FAIL_COND_MSG(err != OK, "Error loading GDExtension configuration file: " + p_path);
	gdextension_path->set_text(p_path);
	// Get configuration keys.
	entry_symbol_edit->set_text(config->get_value("configuration", "entry_symbol", ""));
	compat_max_version_edit->set_text(config->get_value("configuration", "compatibility_maximum", ""));
	compat_min_version_edit->set_text(config->get_value("configuration", "compatibility_minimum", VERSION_BRANCH));
	reloadable_checkbox->set_pressed(config->get_value("configuration", "reloadable", false));
	// Update validation.
	validation_panel->update();
}

void GDExtensionEditDialog::_clear_fields() {
	entry_symbol_edit->clear();
	compat_max_version_edit->clear();
	compat_min_version_edit->clear();
}

void GDExtensionEditDialog::_on_canceled() {
	_clear_fields();
}

void GDExtensionEditDialog::_on_confirmed() {
	// Start with the old config to avoid erasing custom keys.
	const String gdext_res_path = gdextension_path->get_text();
	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(gdext_res_path);
	ERR_FAIL_COND_MSG(err != OK, "Error loading GDExtension configuration file: " + gdext_res_path);
	// Set configuration keys.
	config->set_value("configuration", "entry_symbol", entry_symbol_edit->get_text());
	config->set_value("configuration", "compatibility_minimum", compat_min_version_edit->get_text());
	const String compat_max = compat_max_version_edit->get_text();
	if (!compat_max.is_empty()) {
		config->set_value("configuration", "compatibility_maximum", compat_max);
	}
	const bool is_reloadable = reloadable_checkbox->is_pressed();
	config->set_value("configuration", "reloadable", is_reloadable);
	// Save and reload.
	config->save(gdext_res_path);
	_clear_fields();
	GDExtensionManager *gdext_man = GDExtensionManager::get_singleton();
	gdext_man->get_extension(gdext_res_path)->set_reloadable(is_reloadable);
	if (is_reloadable) {
		gdext_man->reload_extension(gdext_res_path);
	} else {
		WARN_PRINT(TTR("Saved GDExtension changes to ") + gdext_res_path + TTR(", but the extension is not marked as reloadable. You may need to restart Godot to apply changes to the extension."));
	}
}

#define IS_BEFORE_CURRENT_VERSION(m_parts) (m_parts[0] < VERSION_MAJOR || (m_parts[0] == VERSION_MAJOR && (m_parts[1] < VERSION_MINOR || (m_parts[1] == VERSION_MINOR && (m_parts.size() > 2 && m_parts[2] < VERSION_PATCH)))))
#define IS_BEYOND_CURRENT_VERSION(m_parts) (m_parts[0] > VERSION_MAJOR || (m_parts[0] == VERSION_MAJOR && (m_parts[1] > VERSION_MINOR || (m_parts[1] == VERSION_MINOR && (m_parts.size() > 2 && m_parts[2] > VERSION_PATCH)))))

void GDExtensionEditDialog::_on_required_text_changed() {
	// Entry symbol must be a valid programming language identifier.
	String entry_symbol = entry_symbol_edit->get_text();
	if (entry_symbol.is_valid_identifier()) {
		validation_panel->set_message(MSG_ID_ENTRY_SYMBOL_NAME, TTR("Entry symbol is valid (must exist in the initialize_gdextension file)."), EditorValidationPanel::MSG_OK);
	} else {
		validation_panel->set_message(MSG_ID_ENTRY_SYMBOL_NAME, TTR("Entry symbol is not a valid identifier."), EditorValidationPanel::MSG_ERROR);
	}
	// Compatibility minimum is required to be at least 4.1.0.
	String compat_min_text = compat_min_version_edit->get_text();
	Vector<int> compat_min_parts = compat_min_text.split_ints(".");
	if (compat_min_parts.size() < 2) {
		validation_panel->set_message(MSG_ID_COMPAT_MIN_VERSION, TTR("Compat min version must be in X.Y or X.Y.Z format."), EditorValidationPanel::MSG_ERROR);
	} else if (compat_min_parts[0] < 4 || (compat_min_parts[0] == 4 && compat_min_parts[1] == 0)) {
		validation_panel->set_message(MSG_ID_COMPAT_MIN_VERSION, TTR("Compat min version must be at least 4.1.0."), EditorValidationPanel::MSG_ERROR);
	} else if (IS_BEYOND_CURRENT_VERSION(compat_min_parts)) {
		validation_panel->set_message(MSG_ID_COMPAT_MIN_VERSION, TTR("Compat min version cannot be beyond the current Godot version."), EditorValidationPanel::MSG_ERROR);
	} else {
		validation_panel->set_message(MSG_ID_COMPAT_MIN_VERSION, TTR("Compatibility minimum version is valid."), EditorValidationPanel::MSG_OK);
	}
	// Compatibility maximum is optional but must be at least 4.3.0 if set.
	String compat_max_text = compat_max_version_edit->get_text();
	if (compat_max_text.is_empty()) {
		validation_panel->set_message(MSG_ID_COMPAT_MAX_VERSION, TTR("Compatibility maximum version is optional."), EditorValidationPanel::MSG_OK);
	} else {
		Vector<int> compat_max_parts = compat_max_text.split_ints(".");
		if (compat_max_parts.size() < 2) {
			validation_panel->set_message(MSG_ID_COMPAT_MAX_VERSION, TTR("Compat max version must be in X.Y or X.Y.Z format."), EditorValidationPanel::MSG_ERROR);
		} else if (compat_max_parts[0] < 4 || (compat_max_parts[0] == 4 && compat_max_parts[1] < 3)) {
			validation_panel->set_message(MSG_ID_COMPAT_MAX_VERSION, TTR("Compat max version must be at least 4.3.0."), EditorValidationPanel::MSG_ERROR);
		} else if (IS_BEFORE_CURRENT_VERSION(compat_max_parts)) {
			validation_panel->set_message(MSG_ID_COMPAT_MAX_VERSION, TTR("Compat max version cannot be before the current Godot version."), EditorValidationPanel::MSG_ERROR);
		} else {
			validation_panel->set_message(MSG_ID_COMPAT_MAX_VERSION, TTR("Compatibility maximum version is valid."), EditorValidationPanel::MSG_OK);
		}
	}
}

void GDExtensionEditDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			connect(SceneStringName(confirmed), callable_mp(this, &GDExtensionEditDialog::_on_confirmed));
			get_cancel_button()->connect(SceneStringName(pressed), callable_mp(this, &GDExtensionEditDialog::_on_canceled));
		} break;
	}
}

GDExtensionEditDialog::GDExtensionEditDialog() {
	get_ok_button()->set_disabled(true);
	get_ok_button()->set_text(TTR("Save"));
	set_hide_on_ok(true);
	set_title(TTR("Edit GDExtension"));
	set_min_size(Size2(400, 300) * EDSCALE);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(vbox);

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_custom_minimum_size(Size2(400, 150) * EDSCALE);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->add_child(scroll);

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	scroll->add_child(grid);

	// GDExtension file path (read-only).
	Label *gdextension_path_label = memnew(Label);
	gdextension_path_label->set_text(TTR("Path:"));
	gdextension_path_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(gdextension_path_label);

	gdextension_path = memnew(Label);
	gdextension_path->set_text("res://addons/my_extension/my_extension.gdextension");
	gdextension_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(gdextension_path);

	// Entry symbol.
	Label *entry_symbol_label = memnew(Label);
	entry_symbol_label->set_text(TTR("Entry Symbol:"));
	entry_symbol_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(entry_symbol_label);

	entry_symbol_edit = memnew(LineEdit);
	entry_symbol_edit->set_placeholder("libname_library_init");
	entry_symbol_edit->set_tooltip_text(TTR("Required. The symbol to use as the entry point for the extension."));
	entry_symbol_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(entry_symbol_edit);

	// Compatibility Min Version
	Label *compat_min_version_label = memnew(Label);
	compat_min_version_label->set_text(TTR("Compat Min Version:"));
	compat_min_version_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(compat_min_version_label);

	compat_min_version_edit = memnew(LineEdit);
	compat_min_version_edit->set_placeholder(VERSION_BRANCH);
	compat_min_version_edit->set_tooltip_text(TTR("Required. The minimum version of Godot that the extension is compatible with."));
	compat_min_version_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(compat_min_version_edit);

	// Compatibility Max Version
	Label *compat_max_version_label = memnew(Label);
	compat_max_version_label->set_text(TTR("Compat Max Version:"));
	compat_max_version_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(compat_max_version_label);

	compat_max_version_edit = memnew(LineEdit);
	compat_max_version_edit->set_placeholder("<no maximum>");
	compat_max_version_edit->set_tooltip_text(TTR("Optional. The maximum version of Godot that the extension is compatible with."));
	compat_max_version_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(compat_max_version_edit);

	// Reloadable checkbox.
	Label *reloadable_label = memnew(Label);
	reloadable_label->set_text(TTR("Reloadable:"));
	reloadable_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	grid->add_child(reloadable_label);

	reloadable_checkbox = memnew(CheckBox);
	reloadable_checkbox->set_tooltip_text(TTR("Optional. Whether the extension can be reloaded at runtime. Recommended to be true for most extensions."));
	reloadable_checkbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(reloadable_checkbox);

	Control *spacing = memnew(Control);
	spacing->set_custom_minimum_size(Size2(0, 4 * EDSCALE));
	vbox->add_child(spacing);

	validation_panel = memnew(EditorValidationPanel);
	validation_panel->set_custom_minimum_size(Size2(500, 120) * EDSCALE);
	validation_panel->set_v_size_flags(Control::SIZE_SHRINK_END);
	validation_panel->add_line(MSG_ID_ENTRY_SYMBOL_NAME, TTR("Entry symbol is valid (must exist in the initialize_gdextension file)."));
	validation_panel->add_line(MSG_ID_COMPAT_MIN_VERSION, TTR("Compatibility minimum version is valid."));
	validation_panel->add_line(MSG_ID_COMPAT_MAX_VERSION, TTR("Compatibility maximum version is valid."));
	validation_panel->set_update_callback(callable_mp(this, &GDExtensionEditDialog::_on_required_text_changed));
	validation_panel->set_accept_button(get_ok_button());
	vbox->add_child(validation_panel);

	entry_symbol_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	compat_min_version_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	compat_max_version_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
}
