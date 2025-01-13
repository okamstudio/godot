/**************************************************************************/
/*  mesh_instance_3d_editor_plugin.cpp                                    */
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

#include "mesh_instance_3d_editor_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/multi_node_edit.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/3d/navigation_region_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/physics_body_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/gui/aspect_ratio_container.h"
#include "scene/gui/box_container.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/spin_box.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"

void MeshInstance3DEditor::_node_removed(Node *p_node) {
	if (p_node == node) {
		node = nullptr;
		options->hide();
	}
}

void MeshInstance3DEditor::edit(MeshInstance3D *p_mesh) {
	node = p_mesh;
}

Vector<Ref<Shape3D>> MeshInstance3DEditor::create_shape_from_mesh(Ref<Mesh> p_mesh, int p_option, bool p_verbose) {
	Vector<Ref<Shape3D>> shapes;
	switch (p_option) {
		case SHAPE_TYPE_TRIMESH: {
			shapes.push_back(p_mesh->create_trimesh_shape());

			if (p_verbose && shapes.is_empty()) {
				err_dialog->set_text(TTR("Couldn't create a Trimesh collision shape."));
				err_dialog->popup_centered();
			}
		} break;

		case SHAPE_TYPE_SINGLE_CONVEX: {
			shapes.push_back(p_mesh->create_convex_shape(true, false));

			if (p_verbose && shapes.is_empty()) {
				err_dialog->set_text(TTR("Couldn't create a single collision shape."));
				err_dialog->popup_centered();
			}
		} break;

		case SHAPE_TYPE_SIMPLIFIED_CONVEX: {
			shapes.push_back(p_mesh->create_convex_shape(true, true));

			if (p_verbose && shapes.is_empty()) {
				err_dialog->set_text(TTR("Couldn't create a simplified collision shape."));
				err_dialog->popup_centered();
			}
		} break;

		case SHAPE_TYPE_MULTIPLE_CONVEX: {
			Ref<MeshConvexDecompositionSettings> settings;
			settings.instantiate();
			settings->set_max_convex_hulls(32);
			settings->set_max_concavity(0.001);

			shapes = p_mesh->convex_decompose(settings);

			if (p_verbose && shapes.is_empty()) {
				err_dialog->set_text(TTR("Couldn't create any collision shapes."));
				err_dialog->popup_centered();
			}
		} break;

		default:
			break;
	}
	return shapes;
}

void MeshInstance3DEditor::_create_collision_shape() {
	int placement_option = shape_placement->get_selected();
	int shape_type_option = shape_type->get_selected();

	EditorSelection *editor_selection = EditorNode::get_singleton()->get_editor_selection();
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();

	switch (shape_type_option) {
		case SHAPE_TYPE_TRIMESH: {
			ur->create_action(TTR(placement_option == SHAPE_PLACEMENT_SIBLING ? "Create Trimesh Collision Shape Sibling" : "Create Trimesh Static Body"));
		} break;
		case SHAPE_TYPE_SINGLE_CONVEX: {
			ur->create_action(TTR(placement_option == SHAPE_PLACEMENT_SIBLING ? "Create Single Convex Collision Shape Sibling" : "Create Single Convex Static Body"));
		} break;
		case SHAPE_TYPE_SIMPLIFIED_CONVEX: {
			ur->create_action(TTR(placement_option == SHAPE_PLACEMENT_SIBLING ? "Create Simplified Convex Collision Shape Sibling" : "Create Simplified Convex Static Body"));
		} break;
		case SHAPE_TYPE_MULTIPLE_CONVEX: {
			ur->create_action(TTR(placement_option == SHAPE_PLACEMENT_SIBLING ? "Create Multiple Convex Collision Shape Siblings" : "Create Multiple Convex Static Body"));
		} break;
		default:
			break;
	}

	List<Node *> selection = editor_selection->get_selected_node_list();

	bool verbose = false;
	if (selection.is_empty()) {
		selection.push_back(node);
		verbose = true;
	}

	for (Node *E : selection) {
		if (placement_option == SHAPE_PLACEMENT_SIBLING && E == get_tree()->get_edited_scene_root()) {
			if (verbose) {
				err_dialog->set_text(TTR("Can't create a collision shape as sibling for the scene root."));
				err_dialog->popup_centered();
			}
			continue;
		}

		MeshInstance3D *instance = Object::cast_to<MeshInstance3D>(E);
		if (!instance) {
			continue;
		}

		Ref<Mesh> m = instance->get_mesh();
		if (m.is_null()) {
			continue;
		}

		Vector<Ref<Shape3D>> shapes = create_shape_from_mesh(m, shape_type_option, verbose);
		if (shapes.is_empty()) {
			return;
		}

		Node *owner = get_tree()->get_edited_scene_root();
		if (placement_option == SHAPE_PLACEMENT_STATIC_BODY_CHILD) {
			StaticBody3D *body = memnew(StaticBody3D);

			ur->add_do_method(instance, "add_child", body, true);
			ur->add_do_method(body, "set_owner", owner);
			ur->add_do_method(Node3DEditor::get_singleton(), SceneStringName(_request_gizmo), body);

			for (Ref<Shape3D> shape : shapes) {
				CollisionShape3D *cshape = memnew(CollisionShape3D);
				cshape->set_shape(shape);
				body->add_child(cshape, true);
				ur->add_do_method(cshape, "set_owner", owner);
				ur->add_do_method(Node3DEditor::get_singleton(), SceneStringName(_request_gizmo), cshape);
			}
			ur->add_do_reference(body);
			ur->add_undo_method(instance, "remove_child", body);
		} else {
			for (Ref<Shape3D> shape : shapes) {
				CollisionShape3D *cshape = memnew(CollisionShape3D);
				cshape->set_shape(shape);
				cshape->set_name("CollisionShape3D");
				cshape->set_transform(node->get_transform());
				ur->add_do_method(E, "add_sibling", cshape, true);
				ur->add_do_method(cshape, "set_owner", owner);
				ur->add_do_method(Node3DEditor::get_singleton(), SceneStringName(_request_gizmo), cshape);
				ur->add_do_reference(cshape);
				ur->add_undo_method(node->get_parent(), "remove_child", cshape);
			}
		}
	}

	ur->commit_action();
}

void MeshInstance3DEditor::_menu_option(int p_option) {
	Ref<Mesh> mesh = node->get_mesh();
	if (mesh.is_null()) {
		err_dialog->set_text(TTR("Mesh is empty!"));
		err_dialog->popup_centered();
		return;
	}

	switch (p_option) {
		case MENU_OPTION_CREATE_COLLISION_SHAPE: {
			shape_dialog->popup_centered();
		} break;

		case MENU_OPTION_CREATE_NAVMESH: {
			navigation_mesh_dialog->popup_centered(Vector2(200, 90));
		} break;

		case MENU_OPTION_CREATE_OUTLINE_MESH: {
			outline_dialog->popup_centered(Vector2(200, 90));
		} break;
		case MENU_OPTION_CREATE_DEBUG_TANGENTS: {
			EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
			ur->create_action(TTR("Create Debug Tangents"));

			MeshInstance3D *tangents = node->create_debug_tangents_node();

			if (tangents) {
				Node *owner = get_tree()->get_edited_scene_root();

				ur->add_do_reference(tangents);
				ur->add_do_method(node, "add_child", tangents, true);
				ur->add_do_method(tangents, "set_owner", owner);

				ur->add_undo_method(node, "remove_child", tangents);
			}

			ur->commit_action();
		} break;
		case MENU_OPTION_CREATE_UV2: {
			Ref<Mesh> mesh2 = node->get_mesh();
			if (!mesh.is_valid()) {
				err_dialog->set_text(TTR("No mesh to unwrap."));
				err_dialog->popup_centered();
				return;
			}

			// Test if we are allowed to unwrap this mesh resource.
			String path = mesh2->get_path();
			int srpos = path.find("::");
			if (srpos != -1) {
				String base = path.substr(0, srpos);
				if (ResourceLoader::get_resource_type(base) == "PackedScene") {
					if (!get_tree()->get_edited_scene_root() || get_tree()->get_edited_scene_root()->get_scene_file_path() != base) {
						err_dialog->set_text(TTR("Mesh cannot unwrap UVs because it does not belong to the edited scene. Make it unique first."));
						err_dialog->popup_centered();
						return;
					}
				} else {
					if (FileAccess::exists(path + ".import")) {
						err_dialog->set_text(TTR("Mesh cannot unwrap UVs because it belongs to another resource which was imported from another file type. Make it unique first."));
						err_dialog->popup_centered();
						return;
					}
				}
			} else {
				if (FileAccess::exists(path + ".import")) {
					err_dialog->set_text(TTR("Mesh cannot unwrap UVs because it was imported from another file type. Make it unique first."));
					err_dialog->popup_centered();
					return;
				}
			}

			Ref<PrimitiveMesh> primitive_mesh = mesh2;
			if (primitive_mesh.is_valid()) {
				EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
				ur->create_action(TTR("Unwrap UV2"));
				ur->add_do_method(*primitive_mesh, "set_add_uv2", true);
				ur->add_undo_method(*primitive_mesh, "set_add_uv2", primitive_mesh->get_add_uv2());
				ur->commit_action();
			} else {
				Ref<ArrayMesh> array_mesh = mesh2;
				if (!array_mesh.is_valid()) {
					err_dialog->set_text(TTR("Contained Mesh is not of type ArrayMesh."));
					err_dialog->popup_centered();
					return;
				}

				// Preemptively evaluate common fail cases for lightmap unwrapping.
				{
					if (array_mesh->get_blend_shape_count() > 0) {
						err_dialog->set_text(TTR("Can't unwrap mesh with blend shapes."));
						err_dialog->popup_centered();
						return;
					}

					for (int i = 0; i < array_mesh->get_surface_count(); i++) {
						Mesh::PrimitiveType primitive = array_mesh->surface_get_primitive_type(i);

						if (primitive != Mesh::PRIMITIVE_TRIANGLES) {
							err_dialog->set_text(TTR("Only triangles are supported for lightmap unwrap."));
							err_dialog->popup_centered();
							return;
						}

						uint64_t format = array_mesh->surface_get_format(i);
						if (!(format & Mesh::ArrayFormat::ARRAY_FORMAT_NORMAL)) {
							err_dialog->set_text(TTR("Normals are required for lightmap unwrap."));
							err_dialog->popup_centered();
							return;
						}
					}
				}

				Ref<ArrayMesh> unwrapped_mesh = array_mesh->duplicate(false);

				Error err = unwrapped_mesh->lightmap_unwrap(node->get_global_transform());
				if (err != OK) {
					err_dialog->set_text(TTR("UV Unwrap failed, mesh may not be manifold?"));
					err_dialog->popup_centered();
					return;
				}

				EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
				ur->create_action(TTR("Unwrap UV2"));

				ur->add_do_method(node, "set_mesh", unwrapped_mesh);
				ur->add_do_reference(node);
				ur->add_do_reference(array_mesh.ptr());

				ur->add_undo_method(node, "set_mesh", array_mesh);
				ur->add_undo_reference(unwrapped_mesh.ptr());

				ur->commit_action();
			}
		} break;
		case MENU_OPTION_DEBUG_UV1: {
			Ref<Mesh> mesh2 = node->get_mesh();
			if (!mesh2.is_valid()) {
				err_dialog->set_text(TTR("No mesh to debug."));
				err_dialog->popup_centered();
				return;
			}
			_create_uv_lines(0);
		} break;
		case MENU_OPTION_DEBUG_UV2: {
			Ref<Mesh> mesh2 = node->get_mesh();
			if (!mesh2.is_valid()) {
				err_dialog->set_text(TTR("No mesh to debug."));
				err_dialog->popup_centered();
				return;
			}
			_create_uv_lines(1);
		} break;
	}
}

struct MeshInstance3DEditorEdgeSort {
	Vector2 a;
	Vector2 b;

	static uint32_t hash(const MeshInstance3DEditorEdgeSort &p_edge) {
		uint32_t h = hash_murmur3_one_32(HashMapHasherDefault::hash(p_edge.a));
		return hash_fmix32(hash_murmur3_one_32(HashMapHasherDefault::hash(p_edge.b), h));
	}

	bool operator==(const MeshInstance3DEditorEdgeSort &p_b) const {
		return a == p_b.a && b == p_b.b;
	}

	MeshInstance3DEditorEdgeSort() {}
	MeshInstance3DEditorEdgeSort(const Vector2 &p_a, const Vector2 &p_b) {
		if (p_a < p_b) {
			a = p_a;
			b = p_b;
		} else {
			b = p_a;
			a = p_b;
		}
	}
};

void MeshInstance3DEditor::_create_uv_lines(int p_layer) {
	Ref<Mesh> mesh = node->get_mesh();
	ERR_FAIL_COND(!mesh.is_valid());

	HashSet<MeshInstance3DEditorEdgeSort, MeshInstance3DEditorEdgeSort> edges;
	uv_lines.clear();
	for (int i = 0; i < mesh->get_surface_count(); i++) {
		if (mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
			continue;
		}
		Array a = mesh->surface_get_arrays(i);

		Vector<Vector2> uv = a[p_layer == 0 ? Mesh::ARRAY_TEX_UV : Mesh::ARRAY_TEX_UV2];
		if (uv.size() == 0) {
			err_dialog->set_text(vformat(TTR("Mesh has no UV in layer %d."), p_layer + 1));
			err_dialog->popup_centered();
			return;
		}

		const Vector2 *r = uv.ptr();

		Vector<int> indices = a[Mesh::ARRAY_INDEX];
		const int *ri = nullptr;

		int ic;

		if (indices.size()) {
			ic = indices.size();
			ri = indices.ptr();
		} else {
			ic = uv.size();
		}

		for (int j = 0; j < ic; j += 3) {
			for (int k = 0; k < 3; k++) {
				MeshInstance3DEditorEdgeSort edge;
				if (ri) {
					edge.a = r[ri[j + k]];
					edge.b = r[ri[j + ((k + 1) % 3)]];
				} else {
					edge.a = r[j + k];
					edge.b = r[j + ((k + 1) % 3)];
				}

				if (edges.has(edge)) {
					continue;
				}

				uv_lines.push_back(edge.a);
				uv_lines.push_back(edge.b);
				edges.insert(edge);
			}
		}
	}

	debug_uv_dialog->popup_centered();
}

void MeshInstance3DEditor::_debug_uv_draw() {
	if (uv_lines.size() == 0) {
		return;
	}

	debug_uv->set_clip_contents(true);
	debug_uv->draw_rect(
			Rect2(Vector2(), debug_uv->get_size()),
			get_theme_color(SNAME("dark_color_3"), EditorStringName(Editor)));

	// Draw an outline to represent the UV2's beginning and end area (useful on Black OLED theme).
	// Top-left coordinate needs to be `(1, 1)` to prevent `clip_contents` from clipping the top and left lines.
	debug_uv->draw_rect(
			Rect2(Vector2(1, 1), debug_uv->get_size() - Vector2(1, 1)),
			get_theme_color(SNAME("mono_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.125),
			false,
			Math::round(EDSCALE));

	for (int x = 1; x <= 7; x++) {
		debug_uv->draw_line(
				Vector2(debug_uv->get_size().x * 0.125 * x, 0),
				Vector2(debug_uv->get_size().x * 0.125 * x, debug_uv->get_size().y),
				get_theme_color(SNAME("mono_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.125),
				Math::round(EDSCALE));
	}

	for (int y = 1; y <= 7; y++) {
		debug_uv->draw_line(
				Vector2(0, debug_uv->get_size().y * 0.125 * y),
				Vector2(debug_uv->get_size().x, debug_uv->get_size().y * 0.125 * y),
				get_theme_color(SNAME("mono_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.125),
				Math::round(EDSCALE));
	}

	debug_uv->draw_set_transform(Vector2(), 0, debug_uv->get_size());

	// Use a translucent color to allow overlapping triangles to be visible.
	// Divide line width by the drawing scale set above, so that line width is consistent regardless of dialog size.
	// Aspect ratio is preserved by the parent AspectRatioContainer, so we only need to check the X size which is always equal to Y.
	debug_uv->draw_multiline(
			uv_lines,
			get_theme_color(SNAME("mono_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.5),
			Math::round(EDSCALE) / debug_uv->get_size().x);
}

void MeshInstance3DEditor::_create_navigation_mesh() {
	Ref<Mesh> mesh = node->get_mesh();
	if (mesh.is_null()) {
		return;
	}

	Ref<NavigationMesh> nmesh = memnew(NavigationMesh);

	if (nmesh.is_null()) {
		return;
	}

	nmesh->create_from_mesh(mesh);
	NavigationRegion3D *nmi = memnew(NavigationRegion3D);
	nmi->set_navigation_mesh(nmesh);

	Node *owner = get_tree()->get_edited_scene_root();

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Create Navigation Mesh"));

	ur->add_do_method(node, "add_child", nmi, true);
	ur->add_do_method(nmi, "set_owner", owner);
	ur->add_do_method(Node3DEditor::get_singleton(), SceneStringName(_request_gizmo), nmi);

	ur->add_do_reference(nmi);
	ur->add_undo_method(node, "remove_child", nmi);
	ur->commit_action();
}

void MeshInstance3DEditor::_create_outline_mesh() {
	Ref<Mesh> mesh = node->get_mesh();
	if (mesh.is_null()) {
		err_dialog->set_text(TTR("MeshInstance3D lacks a Mesh."));
		err_dialog->popup_centered();
		return;
	}

	if (mesh->get_surface_count() == 0) {
		err_dialog->set_text(TTR("Mesh has no surface to create outlines from."));
		err_dialog->popup_centered();
		return;
	} else if (mesh->get_surface_count() == 1 && mesh->surface_get_primitive_type(0) != Mesh::PRIMITIVE_TRIANGLES) {
		err_dialog->set_text(TTR("Mesh primitive type is not PRIMITIVE_TRIANGLES."));
		err_dialog->popup_centered();
		return;
	}

	Ref<Mesh> mesho = mesh->create_outline(outline_size->get_value());

	if (mesho.is_null()) {
		err_dialog->set_text(TTR("Could not create outline."));
		err_dialog->popup_centered();
		return;
	}

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(mesho);
	Node *owner = get_tree()->get_edited_scene_root();

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();

	ur->create_action(TTR("Create Outline"));

	ur->add_do_method(node, "add_child", mi, true);
	ur->add_do_method(mi, "set_owner", owner);
	ur->add_do_method(Node3DEditor::get_singleton(), SceneStringName(_request_gizmo), mi);

	ur->add_do_reference(mi);
	ur->add_undo_method(node, "remove_child", mi);
	ur->commit_action();
}

void MeshInstance3DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			options->set_button_icon(get_editor_theme_icon(SNAME("MeshInstance3D")));
		} break;
	}
}

MeshInstance3DEditor::MeshInstance3DEditor() {
	options = memnew(MenuButton);
	options->set_text(TTR("Mesh"));
	options->set_switch_on_hover(true);
	Node3DEditor::get_singleton()->add_control_to_menu_panel(options);

	options->get_popup()->add_item(TTR("Create Collision Shape..."), MENU_OPTION_CREATE_COLLISION_SHAPE);
	options->get_popup()->add_item(TTR("Create Navigation Mesh"), MENU_OPTION_CREATE_NAVMESH);
	options->get_popup()->add_separator();
	options->get_popup()->add_item(TTR("Create Outline Mesh..."), MENU_OPTION_CREATE_OUTLINE_MESH);
	options->get_popup()->set_item_tooltip(options->get_popup()->get_item_count() - 1, TTR("Creates a static outline mesh. The outline mesh will have its normals flipped automatically.\nThis can be used instead of the StandardMaterial Grow property when using that property isn't possible."));
	options->get_popup()->add_item(TTR("Create Debug Tangents"), MENU_OPTION_CREATE_DEBUG_TANGENTS);
	options->get_popup()->add_separator();
	options->get_popup()->add_item(TTR("View UV1"), MENU_OPTION_DEBUG_UV1);
	options->get_popup()->add_item(TTR("View UV2"), MENU_OPTION_DEBUG_UV2);
	options->get_popup()->add_item(TTR("Unwrap UV2 for Lightmap/AO"), MENU_OPTION_CREATE_UV2);

	options->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &MeshInstance3DEditor::_menu_option));

	outline_dialog = memnew(ConfirmationDialog);
	outline_dialog->set_title(TTR("Create Outline Mesh"));
	outline_dialog->set_ok_button_text(TTR("Create"));

	VBoxContainer *outline_dialog_vbc = memnew(VBoxContainer);
	outline_dialog->add_child(outline_dialog_vbc);
	//outline_dialog->set_child_rect(outline_dialog_vbc);

	outline_size = memnew(SpinBox);
	outline_size->set_min(0.001);
	outline_size->set_max(1024);
	outline_size->set_step(0.001);
	outline_size->set_value(0.05);
	outline_dialog_vbc->add_margin_child(TTR("Outline Size:"), outline_size);

	add_child(outline_dialog);
	outline_dialog->connect(SceneStringName(confirmed), callable_mp(this, &MeshInstance3DEditor::_create_outline_mesh));

	shape_dialog = memnew(ConfirmationDialog);
	shape_dialog->set_title(TTR("Create Collision Shape"));
	shape_dialog->set_ok_button_text(TTR("Create"));

	VBoxContainer *shape_dialog_vbc = memnew(VBoxContainer);
	shape_dialog->add_child(shape_dialog_vbc);

	Label *l = memnew(Label);
	l->set_text(TTR("Collision Shape placement"));
	shape_dialog_vbc->add_child(l);

	shape_placement = memnew(OptionButton);
	shape_placement->set_h_size_flags(SIZE_EXPAND_FILL);
	shape_placement->add_item(TTR("Sibling"), SHAPE_PLACEMENT_SIBLING);
	shape_placement->set_item_tooltip(-1, TTR("Creates collision shapes as Sibling."));
	shape_placement->add_item(TTR("Static Body Child"), SHAPE_PLACEMENT_STATIC_BODY_CHILD);
	shape_placement->set_item_tooltip(-1, TTR("Creates a StaticBody3D as child and assigns collision shapes to it."));
	shape_dialog_vbc->add_child(shape_placement);

	l = memnew(Label);
	l->set_text(TTR("Collision Shape Type"));
	shape_dialog_vbc->add_child(l);

	shape_type = memnew(OptionButton);
	shape_type->set_h_size_flags(SIZE_EXPAND_FILL);
	shape_type->add_item(TTR("Trimesh"), SHAPE_TYPE_TRIMESH);
	shape_type->set_item_tooltip(-1, TTR("Creates a polygon-based collision shape.\nThis is the most accurate (but slowest) option for collision detection."));
	shape_type->add_item(TTR("Single Convex"), SHAPE_TYPE_SINGLE_CONVEX);
	shape_type->set_item_tooltip(-1, TTR("Creates a single convex collision shape.\nThis is the fastest (but least accurate) option for collision detection."));
	shape_type->add_item(TTR("Simplified Convex"), SHAPE_TYPE_SIMPLIFIED_CONVEX);
	shape_type->set_item_tooltip(-1, TTR("Creates a simplified convex collision shape.\nThis is similar to single collision shape, but can result in a simpler geometry in some cases, at the cost of accuracy."));
	shape_type->add_item(TTR("Multiple Convex"), SHAPE_TYPE_MULTIPLE_CONVEX);
	shape_type->set_item_tooltip(-1, TTR("Creates a polygon-based collision shape.\nThis is a performance middle-ground between a single convex collision and a polygon-based collision."));
	shape_dialog_vbc->add_child(shape_type);

	add_child(shape_dialog);
	shape_dialog->connect(SceneStringName(confirmed), callable_mp(this, &MeshInstance3DEditor::_create_collision_shape));

	err_dialog = memnew(AcceptDialog);
	add_child(err_dialog);

	debug_uv_dialog = memnew(AcceptDialog);
	debug_uv_dialog->set_title(TTR("UV Channel Debug"));
	add_child(debug_uv_dialog);

	debug_uv_arc = memnew(AspectRatioContainer);
	debug_uv_dialog->add_child(debug_uv_arc);

	debug_uv = memnew(Control);
	debug_uv->set_custom_minimum_size(Size2(600, 600) * EDSCALE);
	debug_uv->connect(SceneStringName(draw), callable_mp(this, &MeshInstance3DEditor::_debug_uv_draw));
	debug_uv_arc->add_child(debug_uv);

	navigation_mesh_dialog = memnew(ConfirmationDialog);
	navigation_mesh_dialog->set_title(TTR("Create NavigationMesh"));
	navigation_mesh_dialog->set_ok_button_text(TTR("Create"));

	VBoxContainer *navigation_mesh_dialog_vbc = memnew(VBoxContainer);
	navigation_mesh_dialog->add_child(navigation_mesh_dialog_vbc);

	Label *navigation_mesh_l = memnew(Label);
	navigation_mesh_l->set_text(TTR("Before converting a rendering mesh to a navigation mesh, please verify:\n\n- The mesh is two-dimensional.\n- The mesh has no surface overlap.\n- The mesh has no self-intersection.\n- The mesh surfaces have indices.\n\nIf the mesh does not fulfill these requirements, the pathfinding will be broken."));
	navigation_mesh_dialog_vbc->add_child(navigation_mesh_l);

	add_child(navigation_mesh_dialog);
	navigation_mesh_dialog->connect(SceneStringName(confirmed), callable_mp(this, &MeshInstance3DEditor::_create_navigation_mesh));
}

void MeshInstance3DEditorPlugin::edit(Object *p_object) {
	{
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_object);
		if (mi) {
			mesh_editor->edit(mi);
			return;
		}
	}

	Ref<MultiNodeEdit> mne = Ref<MultiNodeEdit>(p_object);
	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
	if (mne.is_valid() && edited_scene) {
		for (int i = 0; i < mne->get_node_count(); i++) {
			MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(edited_scene->get_node(mne->get_node(i)));
			if (mi) {
				mesh_editor->edit(mi);
				return;
			}
		}
	}
	mesh_editor->edit(nullptr);
}

bool MeshInstance3DEditorPlugin::handles(Object *p_object) const {
	if (Object::cast_to<MeshInstance3D>(p_object)) {
		return true;
	}

	Ref<MultiNodeEdit> mne = Ref<MultiNodeEdit>(p_object);
	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
	if (mne.is_valid() && edited_scene) {
		bool has_mesh = false;
		for (int i = 0; i < mne->get_node_count(); i++) {
			if (Object::cast_to<MeshInstance3D>(edited_scene->get_node(mne->get_node(i)))) {
				if (has_mesh) {
					return true;
				} else {
					has_mesh = true;
				}
			}
		}
	}
	return false;
}

void MeshInstance3DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		mesh_editor->options->show();
	} else {
		mesh_editor->options->hide();
		mesh_editor->edit(nullptr);
	}
}

MeshInstance3DEditorPlugin::MeshInstance3DEditorPlugin() {
	mesh_editor = memnew(MeshInstance3DEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(mesh_editor);

	mesh_editor->options->hide();
}

MeshInstance3DEditorPlugin::~MeshInstance3DEditorPlugin() {
}
