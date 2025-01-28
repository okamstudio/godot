/**************************************************************************/
/*  physical_bone_3d_editor_plugin.cpp                                    */
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

#include "physical_bone_3d_editor_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/physics/physical_bone_3d.h"

void PhysicalBone3DEditor::_on_toggle_button_transform_joint(bool p_is_pressed) {
	_set_move_joint();
}

void PhysicalBone3DEditor::_set_move_joint() {
	if (selected) {
		selected->_set_gizmo_move_joint(button_transform_joint->is_pressed());
		Node3DEditor::get_singleton()->update_transform_gizmo();
	}
}

PhysicalBone3DEditor::PhysicalBone3DEditor() {
	spatial_editor_hb = memnew(HBoxContainer);
	spatial_editor_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	spatial_editor_hb->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
	Node3DEditor::get_singleton()->add_control_to_menu_panel(spatial_editor_hb);

	button_transform_joint = memnew(Button);
	button_transform_joint->set_theme_type_variation(SceneStringName(FlatButton));
	spatial_editor_hb->add_child(button_transform_joint);

	button_transform_joint->set_text(TTR("Move Joint"));
	// TODO: Rework this as a dedicated toolbar control so we can hook into theme changes and update it
	// when the editor theme updates.
	button_transform_joint->set_button_icon(EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("PhysicalBone3D"), EditorStringName(EditorIcons)));
	button_transform_joint->set_toggle_mode(true);
	button_transform_joint->connect(SceneStringName(toggled), callable_mp(this, &PhysicalBone3DEditor::_on_toggle_button_transform_joint));

	hide();
}

void PhysicalBone3DEditor::set_selected(PhysicalBone3D *p_pb) {
	button_transform_joint->set_pressed(false);

	_set_move_joint();
	selected = p_pb;
	_set_move_joint();
}

void PhysicalBone3DEditor::hide() {
	spatial_editor_hb->hide();
}

void PhysicalBone3DEditor::show() {
	spatial_editor_hb->show();
}

PhysicalBone3DEditorPlugin::PhysicalBone3DEditorPlugin() {}

void PhysicalBone3DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		physical_bone_editor.show();
	} else {
		physical_bone_editor.hide();
		physical_bone_editor.set_selected(nullptr);
		selected = nullptr;
	}
}

void PhysicalBone3DEditorPlugin::edit(Object *p_node) {
	PhysicalBone3D *bone = Object::cast_to<PhysicalBone3D>(p_node);
	if (bone) {
		selected = bone;
		physical_bone_editor.set_selected(selected);
	}
}
