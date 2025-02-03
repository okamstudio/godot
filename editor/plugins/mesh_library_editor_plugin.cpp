/**************************************************************************/
/*  mesh_library_editor_plugin.cpp                                        */
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

#include "mesh_library_editor_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "main/main.h"
#include "scene/3d/mesh_instance.h"
#include "scene/3d/navigation_mesh_instance.h"
#include "scene/3d/physics_body.h"
#include "scene/main/viewport.h"
#include "scene/resources/packed_scene.h"
#include "spatial_editor_plugin.h"

void MeshLibraryEditor::edit(const Ref<MeshLibrary> &p_mesh_library) {
	mesh_library = p_mesh_library;
	if (mesh_library.is_valid()) {
		menu->get_popup()->set_item_disabled(menu->get_popup()->get_item_index(MENU_OPTION_UPDATE_FROM_SCENE), !mesh_library->has_meta("_editor_source_scene"));
	}
}

void MeshLibraryEditor::_menu_remove_confirm() {
	switch (option) {
		case MENU_OPTION_REMOVE_ITEM: {
			mesh_library->remove_item(to_erase);
		} break;
		default: {
		};
	}
}

void MeshLibraryEditor::_menu_update_confirm(bool p_apply_xforms) {
	cd_update->hide();
	apply_xforms = p_apply_xforms;
	String existing = mesh_library->get_meta("_editor_source_scene");
	ERR_FAIL_COND(existing == "");
	_import_scene_cbk(existing);
}

void MeshLibraryEditor::_import_scene(Node *p_scene, Ref<MeshLibrary> p_library, bool p_merge, bool p_apply_xforms) {
	if (!p_merge) {
		p_library->clear();
	}

	Map<int, MeshInstance *> mesh_instances;

	for (int i = 0; i < p_scene->get_child_count(); i++) {
		Node *child = p_scene->get_child(i);

		if (!Object::cast_to<MeshInstance>(child)) {
			if (child->get_child_count() > 0) {
				child = child->get_child(0);
				if (!Object::cast_to<MeshInstance>(child)) {
					continue;
				}

			} else {
				continue;
			}
		}

		MeshInstance *mi = Object::cast_to<MeshInstance>(child);
		Ref<Mesh> mesh = mi->get_mesh();
		if (mesh.is_null()) {
			continue;
		}

		mesh = mesh->duplicate();
		for (int j = 0; j < mesh->get_surface_count(); ++j) {
			Ref<Material> mat = mi->get_surface_material(j);

			if (mat.is_valid()) {
				mesh->surface_set_material(j, mat);
			}
		}

		int id = p_library->find_item_by_name(mi->get_name());
		if (id < 0) {
			id = p_library->get_last_unused_item_id();
			p_library->create_item(id);
			p_library->set_item_name(id, mi->get_name());
		}

		p_library->set_item_mesh(id, mesh);

		if (p_apply_xforms) {
			p_library->set_item_mesh_transform(id, mi->get_transform());
		} else {
			p_library->set_item_mesh_transform(id, Transform());
		}

		mesh_instances[id] = mi;

		Vector<MeshLibrary::ShapeData> collisions;

		for (int j = 0; j < mi->get_child_count(); j++) {
			Node *child2 = mi->get_child(j);
			if (!Object::cast_to<StaticBody>(child2)) {
				continue;
			}

			StaticBody *sb = Object::cast_to<StaticBody>(child2);
			List<uint32_t> shapes;
			sb->get_shape_owners(&shapes);

			for (List<uint32_t>::Element *E = shapes.front(); E; E = E->next()) {
				if (sb->is_shape_owner_disabled(E->get())) {
					continue;
				}

				Transform shape_transform;
				if (p_apply_xforms) {
					shape_transform = mi->get_transform();
				}
				shape_transform *= sb->get_transform() * sb->shape_owner_get_transform(E->get());

				for (int k = 0; k < sb->shape_owner_get_shape_count(E->get()); k++) {
					Ref<Shape> collision = sb->shape_owner_get_shape(E->get(), k);
					if (!collision.is_valid()) {
						continue;
					}
					MeshLibrary::ShapeData shape_data;
					shape_data.shape = collision;
					shape_data.local_transform = shape_transform;
					collisions.push_back(shape_data);
				}
			}
		}

		p_library->set_item_shapes(id, collisions);

		Ref<NavigationMesh> navmesh;
		Transform navmesh_transform;
		for (int j = 0; j < mi->get_child_count(); j++) {
			Node *child2 = mi->get_child(j);
			if (!Object::cast_to<NavigationMeshInstance>(child2)) {
				continue;
			}
			NavigationMeshInstance *sb = Object::cast_to<NavigationMeshInstance>(child2);
			navmesh = sb->get_navigation_mesh();
			navmesh_transform = sb->get_transform();
			if (!navmesh.is_null()) {
				break;
			}
		}
		if (!navmesh.is_null()) {
			p_library->set_item_navmesh(id, navmesh);
			p_library->set_item_navmesh_transform(id, navmesh_transform);
		}
	}

	//generate previews!

	if (true) {
		Vector<Ref<Mesh>> meshes;
		Vector<Transform> transforms;
		Vector<int> ids = p_library->get_item_list();
		for (int i = 0; i < ids.size(); i++) {
			if (mesh_instances.find(ids[i])) {
				meshes.push_back(p_library->get_item_mesh(ids[i]));
				transforms.push_back(mesh_instances[ids[i]]->get_transform());
			}
		}

		Vector<Ref<Texture>> textures = EditorInterface::get_singleton()->make_mesh_previews(meshes, &transforms, EditorSettings::get_singleton()->get("editors/grid_map/preview_size"));
		int j = 0;
		for (int i = 0; i < ids.size(); i++) {
			if (mesh_instances.find(ids[i])) {
				p_library->set_item_preview(ids[i], textures[j]);
				j++;
			}
		}
	}
}

void MeshLibraryEditor::_import_scene_cbk(const String &p_str) {
	Ref<PackedScene> ps = ResourceLoader::load(p_str, "PackedScene");
	ERR_FAIL_COND(ps.is_null());
	Node *scene = ps->instance();

	ERR_FAIL_COND_MSG(!scene, "Cannot create an instance from PackedScene '" + p_str + "'.");

	_import_scene(scene, mesh_library, option == MENU_OPTION_UPDATE_FROM_SCENE, apply_xforms);

	memdelete(scene);
	mesh_library->set_meta("_editor_source_scene", p_str);

	menu->get_popup()->set_item_disabled(menu->get_popup()->get_item_index(MENU_OPTION_UPDATE_FROM_SCENE), false);
}

Error MeshLibraryEditor::update_library_file(Node *p_base_scene, Ref<MeshLibrary> ml, bool p_merge, bool p_apply_xforms) {
	_import_scene(p_base_scene, ml, p_merge, p_apply_xforms);
	return OK;
}

void MeshLibraryEditor::_menu_cbk(int p_option) {
	option = p_option;
	switch (p_option) {
		case MENU_OPTION_ADD_ITEM: {
			mesh_library->create_item(mesh_library->get_last_unused_item_id());
		} break;
		case MENU_OPTION_REMOVE_ITEM: {
			String p = editor->get_inspector()->get_selected_path();
			if (p.begins_with("item") && p.get_slice_count("/") >= 2) {
				to_erase = p.get_slice("/", 1).to_int();
				cd_remove->set_text(vformat(TTR("Remove item %d?"), to_erase));
				cd_remove->popup_centered(Size2(300, 60));
			}
		} break;
		case MENU_OPTION_IMPORT_FROM_SCENE: {
			apply_xforms = false;
			file->popup_centered_ratio();
		} break;
		case MENU_OPTION_IMPORT_FROM_SCENE_APPLY_XFORMS: {
			apply_xforms = true;
			file->popup_centered_ratio();
		} break;
		case MENU_OPTION_UPDATE_FROM_SCENE: {
			cd_update->set_text(vformat(TTR("Update from existing scene?:\n%s"), String(mesh_library->get_meta("_editor_source_scene"))));
			cd_update->popup_centered(Size2(500, 60));
		} break;
	}
}

void MeshLibraryEditor::_bind_methods() {
	ClassDB::bind_method("_menu_cbk", &MeshLibraryEditor::_menu_cbk);
	ClassDB::bind_method("_menu_remove_confirm", &MeshLibraryEditor::_menu_remove_confirm);
	ClassDB::bind_method("_menu_update_confirm", &MeshLibraryEditor::_menu_update_confirm);
	ClassDB::bind_method("_import_scene_cbk", &MeshLibraryEditor::_import_scene_cbk);
}

MeshLibraryEditor::MeshLibraryEditor(EditorNode *p_editor) {
	file = memnew(EditorFileDialog);
	file->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	//not for now?
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
	file->clear_filters();
	file->set_title(TTR("Import Scene"));
	for (int i = 0; i < extensions.size(); i++) {
		file->add_filter("*." + extensions[i], extensions[i].to_upper());
	}
	add_child(file);
	file->connect("file_selected", this, "_import_scene_cbk");

	menu = memnew(MenuButton);
	SpatialEditor::get_singleton()->add_control_to_menu_panel(menu);
	menu->set_position(Point2(1, 1));
	menu->set_text(TTR("MeshLibrary"));
	menu->set_icon(EditorNode::get_singleton()->get_gui_base()->get_icon("MeshLibrary", "EditorIcons"));
	menu->get_popup()->add_item(TTR("Add Item"), MENU_OPTION_ADD_ITEM);
	menu->get_popup()->add_item(TTR("Remove Selected Item"), MENU_OPTION_REMOVE_ITEM);
	menu->get_popup()->add_separator();
	menu->get_popup()->add_item(TTR("Import from Scene (Ignore Transforms)"), MENU_OPTION_IMPORT_FROM_SCENE);
	menu->get_popup()->add_item(TTR("Import from Scene (Apply Transforms)"), MENU_OPTION_IMPORT_FROM_SCENE_APPLY_XFORMS);
	menu->get_popup()->add_item(TTR("Update from Scene"), MENU_OPTION_UPDATE_FROM_SCENE);
	menu->get_popup()->set_item_disabled(menu->get_popup()->get_item_index(MENU_OPTION_UPDATE_FROM_SCENE), true);
	menu->get_popup()->connect("id_pressed", this, "_menu_cbk");
	menu->hide();

	editor = p_editor;
	cd_remove = memnew(ConfirmationDialog);
	add_child(cd_remove);
	cd_remove->get_ok()->connect("pressed", this, "_menu_remove_confirm");
	cd_update = memnew(ConfirmationDialog);
	add_child(cd_update);
	cd_update->get_ok()->set_text(TTR("Apply without Transforms"));
	cd_update->get_ok()->connect("pressed", this, "_menu_update_confirm", varray(false));
	cd_update->add_button(TTR("Apply with Transforms"))->connect("pressed", this, "_menu_update_confirm", varray(true));
}

void MeshLibraryEditorPlugin::edit(Object *p_node) {
	if (Object::cast_to<MeshLibrary>(p_node)) {
		mesh_library_editor->edit(Object::cast_to<MeshLibrary>(p_node));
		mesh_library_editor->show();
	} else {
		mesh_library_editor->hide();
	}
}

bool MeshLibraryEditorPlugin::handles(Object *p_node) const {
	return p_node->is_class("MeshLibrary");
}

void MeshLibraryEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		mesh_library_editor->show();
		mesh_library_editor->get_menu_button()->show();
	} else {
		mesh_library_editor->hide();
		mesh_library_editor->get_menu_button()->hide();
	}
}

MeshLibraryEditorPlugin::MeshLibraryEditorPlugin(EditorNode *p_node) {
	mesh_library_editor = memnew(MeshLibraryEditor(p_node));

	p_node->get_viewport()->add_child(mesh_library_editor);
	mesh_library_editor->set_anchors_and_margins_preset(Control::PRESET_TOP_WIDE);
	mesh_library_editor->set_end(Point2(0, 22));
	mesh_library_editor->hide();
}
