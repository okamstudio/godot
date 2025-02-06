/**************************************************************************/
/*  openxr_action_editor.cpp                                              */
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

#include "openxr_action_editor.h"

#include "editor/editor_string_names.h"

void OpenXRActionEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_do_set_name", "name"), &OpenXRActionEditor::_do_set_name);
	ClassDB::bind_method(D_METHOD("_do_set_localized_name", "name"), &OpenXRActionEditor::_do_set_localized_name);
	ClassDB::bind_method(D_METHOD("_do_set_action_type", "type"), &OpenXRActionEditor::_do_set_action_type);

	ADD_SIGNAL(MethodInfo("remove", PropertyInfo(Variant::OBJECT, "action_editor")));
}

void OpenXRActionEditor::_theme_changed() {
	rem_action->set_button_icon(get_theme_icon(SNAME("Remove"), EditorStringName(EditorIcons)));
}

void OpenXRActionEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			_theme_changed();
		} break;
	}
}

void OpenXRActionEditor::_on_action_name_changed(const String p_new_text) {
	if (action->get_name() != p_new_text) {
		undo_redo->create_action(TTR("Rename Action"));
		undo_redo->add_do_method(this, "_do_set_name", p_new_text);
		undo_redo->add_undo_method(this, "_do_set_name", action->get_name());
		undo_redo->commit_action(false);

		// If our localized name matches our action name, set this too
		if (action->get_name() == action->get_localized_name()) {
			undo_redo->create_action(TTR("Rename Actions Localized name"));
			undo_redo->add_do_method(this, "_do_set_localized_name", p_new_text);
			undo_redo->add_undo_method(this, "_do_set_localized_name", action->get_localized_name());
			undo_redo->commit_action(false);

			action->set_localized_name(p_new_text);
			action_localized_name->set_text(p_new_text);
		}
		action->set_name(p_new_text);
		action->set_edited(true);
	}
}

void OpenXRActionEditor::_do_set_name(const String p_new_text) {
	action->set_name(p_new_text);
	action->set_edited(true);
	action_name->set_text(p_new_text);
}

void OpenXRActionEditor::_on_action_localized_name_changed(const String p_new_text) {
	if (action->get_localized_name() != p_new_text) {
		undo_redo->create_action(TTR("Rename Actions Localized name"));
		undo_redo->add_do_method(this, "_do_set_localized_name", p_new_text);
		undo_redo->add_undo_method(this, "_do_set_localized_name", action->get_localized_name());
		undo_redo->commit_action(false);

		action->set_localized_name(p_new_text);
		action->set_edited(true);
	}
}

void OpenXRActionEditor::_do_set_localized_name(const String p_new_text) {
	action->set_localized_name(p_new_text);
	action->set_edited(true);
	action_localized_name->set_text(p_new_text);
}

void OpenXRActionEditor::_on_item_selected(int p_idx) {
	ERR_FAIL_INDEX(p_idx, OpenXRAction::OPENXR_ACTION_MAX);

	OpenXRAction::ActionType action_type = OpenXRAction::ActionType(p_idx);

	if (action->get_action_type() != action_type) {
		undo_redo->create_action(TTR("Change Action Type"));
		undo_redo->add_do_method(this, "_do_set_action_type", action_type);
		undo_redo->add_undo_method(this, "_do_set_action_type", action->get_action_type());
		undo_redo->commit_action(false);

		action->set_action_type(action_type);
		action->set_edited(true);
	}
}

void OpenXRActionEditor::_do_set_action_type(OpenXRAction::ActionType p_action_type) {
	action->set_action_type(p_action_type);
	action->set_edited(true);
	action_type_button->select(int(action->get_action_type()));
}

void OpenXRActionEditor::_on_remove_action() {
	emit_signal("remove", this);
}

OpenXRActionEditor::OpenXRActionEditor(Ref<OpenXRAction> p_action) {
	undo_redo = EditorUndoRedoManager::get_singleton();
	action = p_action;

	set_h_size_flags(Control::SIZE_EXPAND_FILL);

	action_name = memnew(LineEdit);
	action_name->set_text(action->get_name());
	action_name->set_custom_minimum_size(Size2(150.0, 0.0));
	action_name->connect(SceneStringName(text_changed), callable_mp(this, &OpenXRActionEditor::_on_action_name_changed));
	add_child(action_name);

	action_localized_name = memnew(LineEdit);
	action_localized_name->set_text(action->get_localized_name());
	action_localized_name->set_custom_minimum_size(Size2(150.0, 0.0));
	action_localized_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	action_localized_name->connect(SceneStringName(text_changed), callable_mp(this, &OpenXRActionEditor::_on_action_localized_name_changed));
	add_child(action_localized_name);

	action_type_button = memnew(OptionButton);
	action_type_button->add_item("Bool", OpenXRAction::OPENXR_ACTION_BOOL);
	action_type_button->add_item("Float", OpenXRAction::OPENXR_ACTION_FLOAT);
	action_type_button->add_item("Vector2", OpenXRAction::OPENXR_ACTION_VECTOR2);
	action_type_button->add_item("Pose", OpenXRAction::OPENXR_ACTION_POSE);
	action_type_button->add_item("Haptic", OpenXRAction::OPENXR_ACTION_HAPTIC);
	action_type_button->select(int(action->get_action_type()));
	action_type_button->set_custom_minimum_size(Size2(100.0, 0.0));
	action_type_button->connect(SceneStringName(item_selected), callable_mp(this, &OpenXRActionEditor::_on_item_selected));
	add_child(action_type_button);

	// maybe add dropdown to edit our toplevel paths, or do we deduce them from our suggested bindings?

	rem_action = memnew(Button);
	rem_action->set_tooltip_text(TTR("Remove action"));
	rem_action->connect(SceneStringName(pressed), callable_mp(this, &OpenXRActionEditor::_on_remove_action));
	rem_action->set_flat(true);
	add_child(rem_action);
}
