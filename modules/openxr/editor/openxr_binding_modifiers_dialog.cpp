/**************************************************************************/
/*  openxr_binding_modifiers_dialog.cpp                                   */
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

#include "openxr_binding_modifiers_dialog.h"
#include "../action_map/openxr_interaction_profile_metadata.h"
#include "openxr_action_map_editor.h"

void OpenXRBindingModifiersDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_do_add_binding_modifier_editor", "binding_modifier_editor"), &OpenXRBindingModifiersDialog::_do_add_binding_modifier_editor);
	ClassDB::bind_method(D_METHOD("_do_remove_binding_modifier_editor", "binding_modifier_editor"), &OpenXRBindingModifiersDialog::_do_remove_binding_modifier_editor);
}

void OpenXRBindingModifiersDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_create_binding_modifiers();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			if (binding_modifier_sc) {
				binding_modifier_sc->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
			}
		} break;
	}
}

OpenXRBindingModifierEditor *OpenXRBindingModifiersDialog::_add_binding_modifier_editor(Ref<OpenXRBindingModifier> p_binding_modifier) {
	ERR_FAIL_COND_V(p_binding_modifier.is_null(), nullptr);

	String class_name = p_binding_modifier->get_class();
	ERR_FAIL_COND_V(class_name.is_empty(), nullptr);
	String editor_class = OpenXRActionMapEditor::get_binding_modifier_editor_class(class_name);
	ERR_FAIL_COND_V(editor_class.is_empty(), nullptr);

	OpenXRBindingModifierEditor *new_editor = nullptr;

	Object *obj = ClassDB::instantiate(editor_class);
	if (obj) {
		new_editor = Object::cast_to<OpenXRBindingModifierEditor>(obj);
		if (!new_editor) {
			// Not of correct type?? Free it.
			memfree(obj);
		}
	}
	ERR_FAIL_NULL_V(new_editor, nullptr);

	new_editor->setup(action_map, p_binding_modifier);
	new_editor->connect("binding_modifier_removed", callable_mp(this, &OpenXRBindingModifiersDialog::_on_remove_binding_modifier));

	binding_modifiers_vb->add_child(new_editor);
	new_editor->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));

	return new_editor;
}

void OpenXRBindingModifiersDialog::_create_binding_modifiers() {
	Array new_binding_modifiers;

	if (ip_binding.is_valid()) {
		new_binding_modifiers = ip_binding->get_binding_modifiers();
	} else if (interaction_profile.is_valid()) {
		new_binding_modifiers = interaction_profile->get_binding_modifiers();
	} else {
		ERR_FAIL_MSG("No binding nor interaction profile specified.");
	}

	for (int i = 0; i < new_binding_modifiers.size(); i++) {
		Ref<OpenXRBindingModifier> binding_modifier = new_binding_modifiers[i];
		_add_binding_modifier_editor(binding_modifier);
	}
}

void OpenXRBindingModifiersDialog::_on_add_binding_modifier() {
	create_dialog->popup_create(false);
}

void OpenXRBindingModifiersDialog::_on_remove_binding_modifier(Object *p_binding_modifier_editor) {
	if (ip_binding.is_valid()) {
		ip_binding->set_edited(true);
	} else if (interaction_profile.is_valid()) {
		interaction_profile->set_edited(true);
	} else {
		ERR_FAIL_MSG("No binding nor interaction profile specified.");
	}

	OpenXRBindingModifierEditor *binding_modifier_editor = Object::cast_to<OpenXRBindingModifierEditor>(p_binding_modifier_editor);
	ERR_FAIL_NULL(binding_modifier_editor);
	ERR_FAIL_COND(binding_modifier_editor->get_parent() != binding_modifiers_vb);

	undo_redo->create_action(TTR("Remove binding modifier"));
	undo_redo->add_do_method(this, "_do_remove_binding_modifier_editor", binding_modifier_editor);
	undo_redo->add_undo_method(this, "_do_add_binding_modifier_editor", binding_modifier_editor);
	undo_redo->commit_action(true);
}

void OpenXRBindingModifiersDialog::_on_dialog_created() {
	// Instance new binding modifier object
	Variant obj = create_dialog->instantiate_selected();
	ERR_FAIL_COND(obj.get_type() != Variant::OBJECT);

	Ref<OpenXRBindingModifier> new_binding_modifier = obj;
	ERR_FAIL_COND(new_binding_modifier.is_null());

	if (ip_binding.is_valid()) {
		// Add it to our binding.
		ip_binding->add_binding_modifier(new_binding_modifier);
		ip_binding->set_edited(true);
	} else if (interaction_profile.is_valid()) {
		// Add it to our interaction profile.
		interaction_profile->add_binding_modifier(new_binding_modifier);
		interaction_profile->set_edited(true);
	} else {
		ERR_FAIL_MSG("No binding nor interaction profile specified.");
	}

	// Create our editor for this.
	OpenXRBindingModifierEditor *binding_modifier_editor = _add_binding_modifier_editor(new_binding_modifier);
	ERR_FAIL_NULL(binding_modifier_editor);

	// Add undo/redo.
	undo_redo->create_action(TTR("Add binding modifier"));
	undo_redo->add_do_method(this, "_do_add_binding_modifier_editor", binding_modifier_editor);
	undo_redo->add_undo_method(this, "_do_remove_binding_modifier_editor", binding_modifier_editor);
	undo_redo->commit_action(false);
}

void OpenXRBindingModifiersDialog::_do_add_binding_modifier_editor(OpenXRBindingModifierEditor *p_binding_modifier_editor) {
	Ref<OpenXRBindingModifier> binding_modifier = p_binding_modifier_editor->get_binding_modifier();
	ERR_FAIL_COND(binding_modifier.is_null());

	if (ip_binding.is_valid()) {
		// Add it to our binding
		ip_binding->add_binding_modifier(binding_modifier);
	} else if (interaction_profile.is_valid()) {
		// Add it to our interaction profile
		interaction_profile->add_binding_modifier(binding_modifier);
	} else {
		ERR_FAIL_MSG("No binding nor interaction profile specified.");
	}

	binding_modifiers_vb->add_child(p_binding_modifier_editor);
}

void OpenXRBindingModifiersDialog::_do_remove_binding_modifier_editor(OpenXRBindingModifierEditor *p_binding_modifier_editor) {
	Ref<OpenXRBindingModifier> binding_modifier = p_binding_modifier_editor->get_binding_modifier();
	ERR_FAIL_COND(binding_modifier.is_null());

	if (ip_binding.is_valid()) {
		// Remove it from our binding.
		ip_binding->remove_binding_modifier(binding_modifier);
	} else if (interaction_profile.is_valid()) {
		// Removed it to from interaction profile.
		interaction_profile->remove_binding_modifier(binding_modifier);
	} else {
		ERR_FAIL_MSG("No binding nor interaction profile specified.");
	}

	binding_modifiers_vb->remove_child(p_binding_modifier_editor);
}

OpenXRBindingModifiersDialog::OpenXRBindingModifiersDialog() {
	undo_redo = EditorUndoRedoManager::get_singleton();

	set_transient(true);

	binding_modifier_sc = memnew(ScrollContainer);
	binding_modifier_sc->set_custom_minimum_size(Size2(350.0, 0.0));
	binding_modifier_sc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	binding_modifier_sc->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	binding_modifier_sc->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	add_child(binding_modifier_sc);

	binding_modifiers_vb = memnew(VBoxContainer);
	binding_modifiers_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	binding_modifier_sc->add_child(binding_modifiers_vb);

	binding_warning_label = memnew(Label);
	binding_warning_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	binding_warning_label->set_text(TTR("Note: modifiers will only be applied if supported on the host system."));
	binding_modifiers_vb->add_child(binding_warning_label);

	add_binding_modifier_btn = memnew(Button);
	add_binding_modifier_btn->set_text(TTR("Add binding modifier"));
	add_binding_modifier_btn->connect("pressed", callable_mp(this, &OpenXRBindingModifiersDialog::_on_add_binding_modifier));
	binding_modifiers_vb->add_child(add_binding_modifier_btn);

	// TODO may need to create our own dialog for this that can filter on binding modifiers recorded on interaction profiles or on individual bindings.

	create_dialog = memnew(CreateDialog);
	create_dialog->set_transient(true);
	create_dialog->connect("create", callable_mp(this, &OpenXRBindingModifiersDialog::_on_dialog_created));
	add_child(create_dialog);
}

void OpenXRBindingModifiersDialog::setup(Ref<OpenXRActionMap> p_action_map, Ref<OpenXRInteractionProfile> p_interaction_profile, Ref<OpenXRIPBinding> p_ip_binding) {
	OpenXRInteractionProfileMetadata *meta_data = OpenXRInteractionProfileMetadata::get_singleton();
	action_map = p_action_map;
	interaction_profile = p_interaction_profile;
	ip_binding = p_ip_binding;

	String profile_path = interaction_profile->get_interaction_profile_path();

	if (ip_binding.is_valid()) {
		String action_name = "unset";
		String path_name = "unset";

		Ref<OpenXRAction> action = p_ip_binding->get_action();
		if (action.is_valid()) {
			action_name = action->get_name_with_set();
		}

		const OpenXRInteractionProfileMetadata::IOPath *io_path = meta_data->get_io_path(profile_path, p_ip_binding->get_binding_path());
		if (io_path != nullptr) {
			path_name = io_path->display_name;
		}

		create_dialog->set_base_type("OpenXRActionBindingModifier");
		set_title(TTR("Binding modifiers for:") + " " + action_name + ": " + path_name);
	} else if (interaction_profile.is_valid()) {
		String profile_name = profile_path;

		const OpenXRInteractionProfileMetadata::InteractionProfile *profile_def = meta_data->get_profile(profile_path);
		if (profile_def != nullptr) {
			profile_name = profile_def->display_name;
		}

		create_dialog->set_base_type("OpenXRIPBindingModifier");
		set_title(TTR("Binding modifiers for:") + " " + profile_name);
	}
}
