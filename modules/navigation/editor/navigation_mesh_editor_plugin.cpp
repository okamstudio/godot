/**************************************************************************/
/*  navigation_mesh_editor_plugin.cpp                                     */
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

#include "navigation_mesh_editor_plugin.h"

#ifdef TOOLS_ENABLED

#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/navigation_region_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"

void NavigationMeshEditor::_node_removed(Node *p_node) {
	if (p_node == node) {
		node = nullptr;

		hide();
	}
}

void NavigationMeshEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			button_bake->set_button_icon(get_theme_icon(SNAME("Bake"), EditorStringName(EditorIcons)));
			button_reset->set_button_icon(get_theme_icon(SNAME("Reload"), EditorStringName(EditorIcons)));
		} break;
	}
}

void NavigationMeshEditor::_bake_pressed() {
	button_bake->set_pressed(false);

	ERR_FAIL_NULL(node);
	Ref<NavigationMesh> navmesh = node->get_navigation_mesh();
	if (!navmesh.is_valid()) {
		err_dialog->set_text(TTR("A NavigationMesh resource must be set or created for this node to work."));
		err_dialog->popup_centered();
		return;
	}

	String path = navmesh->get_path();
	if (!path.is_resource_file()) {
		int srpos = path.find("::");
		if (srpos != -1) {
			String base = path.substr(0, srpos);
			if (ResourceLoader::get_resource_type(base) == "PackedScene") {
				if (!get_tree()->get_edited_scene_root() || get_tree()->get_edited_scene_root()->get_scene_file_path() != base) {
					err_dialog->set_text(TTR("Cannot generate navigation mesh because it does not belong to the edited scene. Make it unique first."));
					err_dialog->popup_centered();
					return;
				}
			} else {
				if (FileAccess::exists(base + ".import")) {
					err_dialog->set_text(TTR("Cannot generate navigation mesh because it belongs to a resource which was imported."));
					err_dialog->popup_centered();
					return;
				}
			}
		}
	} else {
		if (FileAccess::exists(path + ".import")) {
			err_dialog->set_text(TTR("Cannot generate navigation mesh because the resource was imported from another type."));
			err_dialog->popup_centered();
			return;
		}
	}

	node->bake_navigation_mesh(true);

	node->update_gizmos();
}

void NavigationMeshEditor::_clear_pressed() {
	if (node) {
		if (node->get_navigation_mesh().is_valid()) {
			node->get_navigation_mesh()->clear();
		}
	}

	button_bake->set_pressed(false);
	bake_info->set_text("");

	if (node) {
		node->update_gizmos();
	}
}

void NavigationMeshEditor::edit(NavigationRegion3D *p_nav_region) {
	if (p_nav_region == nullptr || node == p_nav_region) {
		return;
	}

	node = p_nav_region;
}

NavigationMeshEditor::NavigationMeshEditor() {
	bake_hbox = memnew(HBoxContainer);

	button_bake = memnew(Button);
	button_bake->set_theme_type_variation(SceneStringName(FlatButton));
	bake_hbox->add_child(button_bake);
	button_bake->set_toggle_mode(true);
	button_bake->set_text(TTR("Bake NavigationMesh"));
	button_bake->set_tooltip_text(TTR("Bakes the NavigationMesh by first parsing the scene for source geometry and then creating the navigation mesh vertices and polygons."));
	button_bake->connect(SceneStringName(pressed), callable_mp(this, &NavigationMeshEditor::_bake_pressed));

	button_reset = memnew(Button);
	button_reset->set_theme_type_variation(SceneStringName(FlatButton));
	bake_hbox->add_child(button_reset);
	button_reset->set_text(TTR("Clear NavigationMesh"));
	button_reset->set_tooltip_text(TTR("Clears the internal NavigationMesh vertices and polygons."));
	button_reset->connect(SceneStringName(pressed), callable_mp(this, &NavigationMeshEditor::_clear_pressed));

	bake_info = memnew(Label);
	bake_hbox->add_child(bake_info);

	err_dialog = memnew(AcceptDialog);
	add_child(err_dialog);
	node = nullptr;
}

NavigationMeshEditor::~NavigationMeshEditor() {
}

void NavigationMeshEditorPlugin::edit(Object *p_object) {
	navigation_mesh_editor->edit(Object::cast_to<NavigationRegion3D>(p_object));
}

bool NavigationMeshEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("NavigationRegion3D");
}

void NavigationMeshEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		navigation_mesh_editor->show();
		navigation_mesh_editor->bake_hbox->show();
	} else {
		navigation_mesh_editor->hide();
		navigation_mesh_editor->bake_hbox->hide();
		navigation_mesh_editor->edit(nullptr);
	}
}

NavigationMeshEditorPlugin::NavigationMeshEditorPlugin() {
	navigation_mesh_editor = memnew(NavigationMeshEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(navigation_mesh_editor);
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, navigation_mesh_editor->bake_hbox);
	navigation_mesh_editor->hide();
	navigation_mesh_editor->bake_hbox->hide();
}

NavigationMeshEditorPlugin::~NavigationMeshEditorPlugin() {
}

#endif // TOOLS_ENABLED
