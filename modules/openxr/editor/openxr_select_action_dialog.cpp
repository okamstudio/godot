/**************************************************************************/
/*  openxr_select_action_dialog.cpp                                       */
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

#include "openxr_select_action_dialog.h"

void OpenXRSelectActionDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("action_selected", PropertyInfo(Variant::STRING, "action")));
}

void OpenXRSelectActionDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			scroll->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
		} break;
	}
}

void OpenXRSelectActionDialog::_on_select_action(const String p_action) {
	if (selected_action != "") {
		NodePath button_path = action_buttons[selected_action];
		Button *button = Object::cast_to<Button>(get_node(button_path));
		if (button != nullptr) {
			button->set_flat(true);
		}
	}

	selected_action = p_action;

	if (selected_action != "") {
		NodePath button_path = action_buttons[selected_action];
		Button *button = Object::cast_to<Button>(get_node(button_path));
		if (button != nullptr) {
			button->set_flat(false);
		}
	}
}

void OpenXRSelectActionDialog::open() {
	ERR_FAIL_COND(action_map.is_null());

	// Out with the old.
	while (main_vb->get_child_count() > 0) {
		memdelete(main_vb->get_child(0));
	}

	selected_action = "";
	action_buttons.clear();

	// In with the new.
	Array action_sets = action_map->get_action_sets();
	for (int i = 0; i < action_sets.size(); i++) {
		Ref<OpenXRActionSet> action_set = action_sets[i];

		Label *action_set_label = memnew(Label);
		action_set_label->set_text(action_set->get_localized_name());
		main_vb->add_child(action_set_label);

		Array actions = action_set->get_actions();
		for (int j = 0; j < actions.size(); j++) {
			Ref<OpenXRAction> action = actions[j];

			HBoxContainer *action_hb = memnew(HBoxContainer);
			main_vb->add_child(action_hb);

			Control *indent_node = memnew(Control);
			indent_node->set_custom_minimum_size(Size2(10.0, 0.0));
			action_hb->add_child(indent_node);

			Button *action_button = memnew(Button);
			String action_name = action->get_name_with_set();
			action_button->set_flat(true);
			action_button->set_text(action->get_name() + ": " + action->get_localized_name());
			action_button->connect(SceneStringName(pressed), callable_mp(this, &OpenXRSelectActionDialog::_on_select_action).bind(action_name));
			action_hb->add_child(action_button);

			action_buttons[action_name] = action_button->get_path();
		}
	}

	popup_centered();
}

void OpenXRSelectActionDialog::ok_pressed() {
	if (selected_action == "") {
		return;
	}

	emit_signal("action_selected", selected_action);

	hide();
}

OpenXRSelectActionDialog::OpenXRSelectActionDialog(Ref<OpenXRActionMap> p_action_map) {
	action_map = p_action_map;

	set_title(TTR("Select an action"));

	scroll = memnew(ScrollContainer);
	scroll->set_custom_minimum_size(Size2(600.0, 400.0));
	add_child(scroll);

	main_vb = memnew(VBoxContainer);
	main_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->add_child(main_vb);
}
