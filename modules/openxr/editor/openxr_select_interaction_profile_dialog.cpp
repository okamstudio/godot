/**************************************************************************/
/*  openxr_select_interaction_profile_dialog.cpp                          */
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

#include "openxr_select_interaction_profile_dialog.h"

#include "../action_map/openxr_interaction_profile_metadata.h"
#include "../openxr_api.h"

void OpenXRSelectInteractionProfileDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("interaction_profile_selected", PropertyInfo(Variant::STRING, "interaction_profile")));
}

void OpenXRSelectInteractionProfileDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			scroll->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
		} break;
	}
}

void OpenXRSelectInteractionProfileDialog::_on_select_interaction_profile(const String p_interaction_profile) {
	if (selected_interaction_profile != "") {
		NodePath button_path = ip_buttons[selected_interaction_profile];
		Button *button = Object::cast_to<Button>(get_node(button_path));
		if (button != nullptr) {
			button->set_flat(true);
		}
	}

	selected_interaction_profile = p_interaction_profile;

	if (selected_interaction_profile != "") {
		NodePath button_path = ip_buttons[selected_interaction_profile];
		Button *button = Object::cast_to<Button>(get_node(button_path));
		if (button != nullptr) {
			button->set_flat(false);
		}
	}
}

void OpenXRSelectInteractionProfileDialog::open(PackedStringArray p_do_not_include) {
	int available_count = 0;

	OpenXRInteractionProfileMetadata *meta_data = OpenXRInteractionProfileMetadata::get_singleton();
	ERR_FAIL_NULL(meta_data);

	// Out with the old.
	while (main_vb->get_child_count() > 1) {
		memdelete(main_vb->get_child(1));
	}

	PackedStringArray requested_extensions = OpenXRAPI::get_all_requested_extensions();

	selected_interaction_profile = "";
	ip_buttons.clear();

	// In with the new.
	PackedStringArray interaction_profiles = meta_data->get_interaction_profile_paths();
	for (const String &path : interaction_profiles) {
		const String extension = meta_data->get_interaction_profile_extension(path);
		if (!p_do_not_include.has(path) && (extension.is_empty() || requested_extensions.has(extension))) {
			Button *ip_button = memnew(Button);
			ip_button->set_flat(true);
			ip_button->set_text(meta_data->get_profile(path)->display_name);
			ip_button->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
			ip_button->connect(SceneStringName(pressed), callable_mp(this, &OpenXRSelectInteractionProfileDialog::_on_select_interaction_profile).bind(path));
			main_vb->add_child(ip_button);

			ip_buttons[path] = ip_button->get_path();
			available_count++;
		}
	}

	all_selected->set_visible(available_count == 0);
	get_cancel_button()->set_visible(available_count > 0);
	popup_centered();
}

void OpenXRSelectInteractionProfileDialog::ok_pressed() {
	if (selected_interaction_profile != "") {
		emit_signal("interaction_profile_selected", selected_interaction_profile);
	}

	hide();
}

OpenXRSelectInteractionProfileDialog::OpenXRSelectInteractionProfileDialog() {
	set_title(TTR("Select an interaction profile"));

	scroll = memnew(ScrollContainer);
	scroll->set_custom_minimum_size(Size2(600.0, 400.0));
	add_child(scroll);

	main_vb = memnew(VBoxContainer);
	main_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->add_child(main_vb);

	all_selected = memnew(Label);
	all_selected->set_text(TTR("All interaction profiles have been added to the action map."));
	main_vb->add_child(all_selected);
}
