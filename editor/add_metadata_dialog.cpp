/**************************************************************************/
/*  add_metadata_dialog.cpp                                               */
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

#include "add_metadata_dialog.h"

#include "editor/themes/editor_scale.h"

AddMetadataDialog::AddMetadataDialog() {
	VBoxContainer *vbc = memnew(VBoxContainer);
	add_child(vbc);

	HBoxContainer *hbc = memnew(HBoxContainer);
	vbc->add_child(hbc);
	hbc->add_child(memnew(Label(TTR("Name:"))));

	add_meta_name = memnew(LineEdit);
	add_meta_name->set_custom_minimum_size(Size2(200 * EDSCALE, 1));
	hbc->add_child(add_meta_name);
	hbc->add_child(memnew(Label(TTR("Type:"))));

	add_meta_type = memnew(OptionButton);

	hbc->add_child(add_meta_type);

	Control *spacing = memnew(Control);
	vbc->add_child(spacing);
	spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

	HBoxContainer *typed_container = memnew(HBoxContainer);
	typed_container->hide();

	Label *key_label = memnew(Label);
	typed_container->add_child(key_label);

	add_meta_key_type = memnew(OptionButton);
	typed_container->add_child(add_meta_key_type);

	Label *value_label = memnew(Label(TTR("Value:")));
	typed_container->add_child(value_label);

	add_meta_value_type = memnew(OptionButton);
	typed_container->add_child(add_meta_value_type);

	vbc->add_child(typed_container);

	Control *spacing2 = memnew(Control);
	vbc->add_child(spacing2);
	spacing2->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

	set_ok_button_text(TTR("Add"));
	register_text_enter(add_meta_name);

	validation_panel = memnew(EditorValidationPanel);
	vbc->add_child(validation_panel);
	validation_panel->add_line(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Metadata name is valid."));
	validation_panel->set_update_callback(callable_mp(this, &AddMetadataDialog::_check_meta_name));
	validation_panel->set_accept_button(get_ok_button());

	add_meta_name->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	add_meta_type->connect(SceneStringName(item_selected), callable_mp(this, &AddMetadataDialog::_update_controls).bind(typed_container, key_label, value_label).unbind(1));
}

void AddMetadataDialog::_complete_init(const StringName &p_title) {
	add_meta_name->set_text("");
	validation_panel->update();

	set_title(vformat(TTR("Add Metadata Property for \"%s\""), p_title));

	// Skip if we already completed the initialization.
	if (add_meta_type->get_item_count()) {
		return;
	}

	// Theme icons can be retrieved only the Window has been initialized.
	// Need to be able to set untyped arrays and dictionaries.
	add_meta_key_type->add_icon_item(get_editor_theme_icon("Variant"), "Variant", 0);
	add_meta_value_type->add_icon_item(get_editor_theme_icon("Variant"), "Variant", 0);

	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		if (i == Variant::NIL || i == Variant::RID || i == Variant::CALLABLE || i == Variant::SIGNAL) {
			continue; //not editable by inspector.
		}
		String type = i == Variant::OBJECT ? String("Resource") : Variant::get_type_name(Variant::Type(i));

		add_meta_type->add_icon_item(get_editor_theme_icon(type), type, i);
		add_meta_key_type->add_icon_item(get_editor_theme_icon(type), type, i);
		add_meta_value_type->add_icon_item(get_editor_theme_icon(type), type, i);
	}
}

// Parameters bound in init to prevent needing extra private variables just for this purpose.
void AddMetadataDialog::_update_controls(HBoxContainer *p_typed_container, Label *p_key_label, Label *p_value_label) {
	if (add_meta_type->get_selected_id() == Variant::DICTIONARY) {
		p_value_label->show();
		add_meta_value_type->show();
		p_key_label->set_text(TTR("Key:"));
		p_typed_container->show();
	} else if (add_meta_type->get_selected_id() == Variant::ARRAY) {
		p_value_label->hide();
		add_meta_value_type->hide();
		p_key_label->set_text(TTR("Element:"));
		p_typed_container->show();
	} else {
		p_typed_container->hide();
	}
}

void AddMetadataDialog::open(const StringName p_title, List<StringName> &p_existing_metas) {
	this->_existing_metas = p_existing_metas;
	_complete_init(p_title);
	popup_centered();
	add_meta_name->grab_focus();
}

StringName AddMetadataDialog::get_meta_name() {
	return add_meta_name->get_text();
}

Variant AddMetadataDialog::get_meta_defval() {
	const int type_id = add_meta_type->get_selected_id();

	if (type_id == Variant::ARRAY) {
		const int elem_type_id = add_meta_key_type->get_selected_id();

		Array defarray = Array();
		// Do not set Array[Variant] in metadata since untyped Array serves same purpose.
		if (elem_type_id != Variant::NIL) {
			defarray.set_typed(elem_type_id, elem_type_id == Variant::OBJECT ? "Resource" : "", Variant::NIL);
		}
		return defarray;
	} else if (type_id == Variant::DICTIONARY) {
		const int key_type_id = add_meta_key_type->get_selected_id();
		const int value_type_id = add_meta_value_type->get_selected_id();

		Dictionary defdict = Dictionary();
		// Can set a Variant here if the other is typed.
		if (key_type_id != Variant::NIL || value_type_id != Variant::NIL) {
			defdict.set_typed(key_type_id, key_type_id == Variant::OBJECT ? "Resource" : "", Variant::NIL, value_type_id, value_type_id == Variant::OBJECT ? "Resource" : "", Variant::NIL);
		}
		return defdict;
	}
	Variant defval;
	Callable::CallError ce;
	Variant::construct(Variant::Type(type_id), defval, nullptr, 0, ce);
	return defval;
}

void AddMetadataDialog::_check_meta_name() {
	const String meta_name = add_meta_name->get_text();

	if (meta_name.is_empty()) {
		validation_panel->set_message(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Metadata name can't be empty."), EditorValidationPanel::MSG_ERROR);
	} else if (!meta_name.is_valid_ascii_identifier()) {
		validation_panel->set_message(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Metadata name must be a valid identifier."), EditorValidationPanel::MSG_ERROR);
	} else if (_existing_metas.find(meta_name)) {
		validation_panel->set_message(EditorValidationPanel::MSG_ID_DEFAULT, vformat(TTR("Metadata with name \"%s\" already exists."), meta_name), EditorValidationPanel::MSG_ERROR);
	} else if (meta_name[0] == '_') {
		validation_panel->set_message(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Names starting with _ are reserved for editor-only metadata."), EditorValidationPanel::MSG_ERROR);
	}
}
