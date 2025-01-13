/**************************************************************************/
/*  scene_create_dialog.cpp                                               */
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

#include "scene_create_dialog.h"

#include "core/io/dir_access.h"
#include "editor/create_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/resources/packed_scene.h"

void SceneCreateDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			select_node_button->set_button_icon(get_editor_theme_icon(SNAME("ClassList")));
			node_type_2d->set_button_icon(get_editor_theme_icon(SNAME("Node2D")));
			node_type_3d->set_button_icon(get_editor_theme_icon(SNAME("Node3D")));
			node_type_gui->set_button_icon(get_editor_theme_icon(SNAME("Control")));
			node_type_other->add_theme_icon_override(SNAME("icon"), get_editor_theme_icon(SNAME("Node")));
		} break;

		case NOTIFICATION_READY: {
			select_node_dialog->select_base();
		} break;
	}
}

void SceneCreateDialog::config(const String &p_dir) {
	directory = p_dir;
	root_name_edit->set_text("");
	scene_name_edit->set_text("");
	callable_mp((Control *)scene_name_edit, &Control::grab_focus).call_deferred();
	validation_panel->update();
}

void SceneCreateDialog::accept_create() {
	if (!get_ok_button()->is_disabled()) {
		hide();
		emit_signal(SceneStringName(confirmed));
	}
}

void SceneCreateDialog::browse_types() {
	select_node_dialog->popup_create(true);
	select_node_dialog->set_title(TTR("Pick Root Node Type"));
	select_node_dialog->set_ok_button_text(TTR("Pick"));
}

void SceneCreateDialog::on_type_picked() {
	other_type_display->set_text(select_node_dialog->get_selected_type().get_slice(" ", 0));
	if (node_type_other->is_pressed()) {
		validation_panel->update();
	} else {
		node_type_other->set_pressed(true); // Calls validation_panel->update() via group.
	}
}

void SceneCreateDialog::update_dialog() {
	scene_name = scene_name_edit->get_text().strip_edges();

	if (scene_name.is_empty()) {
		validation_panel->set_message(MSG_ID_PATH, TTR("Scene name is empty."), EditorValidationPanel::MSG_ERROR);
	}

	if (validation_panel->is_valid()) {
		if (!scene_name.ends_with(".")) {
			scene_name += ".";
		}
		scene_name += scene_extension_picker->get_selected_metadata().operator String();
	}

	if (validation_panel->is_valid() && !scene_name.is_valid_filename()) {
		validation_panel->set_message(MSG_ID_PATH, TTR("File name invalid."), EditorValidationPanel::MSG_ERROR);
	} else if (validation_panel->is_valid() && scene_name[0] == '.') {
		validation_panel->set_message(MSG_ID_PATH, TTR("File name begins with a dot."), EditorValidationPanel::MSG_ERROR);
	}

	if (validation_panel->is_valid()) {
		scene_name = directory.path_join(scene_name);
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (da->file_exists(scene_name)) {
			validation_panel->set_message(MSG_ID_PATH, TTR("File already exists."), EditorValidationPanel::MSG_ERROR);
		}
	}

	const StringName root_type_name = StringName(other_type_display->get_text());
	if (has_theme_icon(root_type_name, EditorStringName(EditorIcons))) {
		node_type_other->set_button_icon(get_editor_theme_icon(root_type_name));
	} else {
		node_type_other->set_button_icon(nullptr);
	}

	root_name = root_name_edit->get_text().strip_edges();
	if (root_name.is_empty()) {
		root_name = scene_name_edit->get_text().strip_edges();

		if (root_name.is_empty()) {
			root_name_edit->set_placeholder(TTR("Leave empty to derive from scene name"));
		} else {
			// Respect the desired root node casing from ProjectSettings.
			root_name = Node::adjust_name_casing(root_name);
			root_name_edit->set_placeholder(root_name.validate_node_name());
		}
	}

	if (root_name.is_empty()) {
		validation_panel->set_message(MSG_ID_ROOT, TTR("Invalid root node name."), EditorValidationPanel::MSG_ERROR);
	} else if (root_name != root_name.validate_node_name()) {
		validation_panel->set_message(MSG_ID_ROOT, TTR("Invalid root node name characters have been replaced."), EditorValidationPanel::MSG_WARNING);
	}
}

String SceneCreateDialog::get_scene_path() const {
	return scene_name;
}

Node *SceneCreateDialog::create_scene_root() {
	ERR_FAIL_NULL_V(node_type_group->get_pressed_button(), nullptr);
	RootType type = (RootType)node_type_group->get_pressed_button()->get_meta(type_meta).operator int();

	Node *root = nullptr;
	switch (type) {
		case ROOT_2D_SCENE:
			root = memnew(Node2D);
			break;
		case ROOT_3D_SCENE:
			root = memnew(Node3D);
			break;
		case ROOT_USER_INTERFACE: {
			Control *gui_ctl = memnew(Control);
			// Making the root control full rect by default is more useful for resizable UIs.
			gui_ctl->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
			gui_ctl->set_grow_direction_preset(Control::PRESET_FULL_RECT);
			root = gui_ctl;
		} break;
		case ROOT_OTHER:
			root = Object::cast_to<Node>(select_node_dialog->instantiate_selected());
			break;
	}

	ERR_FAIL_NULL_V(root, nullptr);
	root->set_name(root_name);
	return root;
}

SceneCreateDialog::SceneCreateDialog() {
	select_node_dialog = memnew(CreateDialog);
	add_child(select_node_dialog);
	select_node_dialog->set_base_type("Node");
	select_node_dialog->connect("create", callable_mp(this, &SceneCreateDialog::on_type_picked));

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);

	GridContainer *gc = memnew(GridContainer);
	main_vb->add_child(gc);
	gc->set_columns(2);

	{
		Label *label = memnew(Label(TTR("Root Type:")));
		gc->add_child(label);
		label->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);

		VBoxContainer *vb = memnew(VBoxContainer);
		gc->add_child(vb);

		node_type_group.instantiate();

		node_type_2d = memnew(CheckBox);
		vb->add_child(node_type_2d);
		node_type_2d->set_text(TTR("2D Scene"));
		node_type_2d->set_button_group(node_type_group);
		node_type_2d->set_meta(type_meta, ROOT_2D_SCENE);
		node_type_2d->set_pressed(true);

		node_type_3d = memnew(CheckBox);
		vb->add_child(node_type_3d);
		node_type_3d->set_text(TTR("3D Scene"));
		node_type_3d->set_button_group(node_type_group);
		node_type_3d->set_meta(type_meta, ROOT_3D_SCENE);

		node_type_gui = memnew(CheckBox);
		vb->add_child(node_type_gui);
		node_type_gui->set_text(TTR("User Interface"));
		node_type_gui->set_button_group(node_type_group);
		node_type_gui->set_meta(type_meta, ROOT_USER_INTERFACE);

		HBoxContainer *hb = memnew(HBoxContainer);
		vb->add_child(hb);

		node_type_other = memnew(CheckBox);
		hb->add_child(node_type_other);
		node_type_other->set_button_group(node_type_group);
		node_type_other->set_meta(type_meta, ROOT_OTHER);

		Control *spacing = memnew(Control);
		hb->add_child(spacing);
		spacing->set_custom_minimum_size(Size2(4 * EDSCALE, 0));

		other_type_display = memnew(LineEdit);
		hb->add_child(other_type_display);
		other_type_display->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		other_type_display->set_editable(false);
		other_type_display->set_text("Node");

		select_node_button = memnew(Button);
		hb->add_child(select_node_button);
		select_node_button->connect(SceneStringName(pressed), callable_mp(this, &SceneCreateDialog::browse_types));
	}

	{
		Label *label = memnew(Label(TTR("Scene Name:")));
		gc->add_child(label);

		HBoxContainer *hb = memnew(HBoxContainer);
		gc->add_child(hb);

		scene_name_edit = memnew(LineEdit);
		hb->add_child(scene_name_edit);
		scene_name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		scene_name_edit->connect(SceneStringName(text_submitted), callable_mp(this, &SceneCreateDialog::accept_create).unbind(1));

		List<String> extensions;
		Ref<PackedScene> sd = memnew(PackedScene);
		ResourceSaver::get_recognized_extensions(sd, &extensions);

		scene_extension_picker = memnew(OptionButton);
		hb->add_child(scene_extension_picker);
		for (const String &E : extensions) {
			scene_extension_picker->add_item("." + E);
			scene_extension_picker->set_item_metadata(-1, E);
		}
	}

	{
		Label *label = memnew(Label(TTR("Root Name:")));
		gc->add_child(label);

		root_name_edit = memnew(LineEdit);
		gc->add_child(root_name_edit);
		root_name_edit->set_tooltip_text(TTR("When empty, the root node name is derived from the scene name based on the \"editor/naming/node_name_casing\" project setting."));
		root_name_edit->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		root_name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		root_name_edit->connect(SceneStringName(text_submitted), callable_mp(this, &SceneCreateDialog::accept_create).unbind(1));
	}

	Control *spacing = memnew(Control);
	main_vb->add_child(spacing);
	spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

	validation_panel = memnew(EditorValidationPanel);
	main_vb->add_child(validation_panel);
	validation_panel->add_line(MSG_ID_PATH, TTR("Scene name is valid."));
	validation_panel->add_line(MSG_ID_ROOT, TTR("Root node valid."));
	validation_panel->set_update_callback(callable_mp(this, &SceneCreateDialog::update_dialog));
	validation_panel->set_accept_button(get_ok_button());

	node_type_group->connect(SceneStringName(pressed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	scene_name_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
	root_name_edit->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));

	set_title(TTR("Create New Scene"));
	set_min_size(Size2i(400 * EDSCALE, 0));
}
