/**************************************************************************/
/*  mesh_editor_uv_tools.cpp                                              */
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

#include "mesh_editor_uv_tools.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/aspect_ratio_container.h"
#include "scene/gui/dialogs.h"
#include "scene/resources/3d/primitive_meshes.h"

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

void MeshEditorUVTools::create_uv_lines(Ref<Mesh> p_mesh, int p_layer) {
	ERR_FAIL_COND(p_mesh.is_null());

	HashSet<MeshInstance3DEditorEdgeSort, MeshInstance3DEditorEdgeSort> edges;
	uv_lines.clear();
	for (int i = 0; i < p_mesh->get_surface_count(); i++) {
		if (p_mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
			continue;
		}
		Array a = p_mesh->surface_get_arrays(i);

		Vector<Vector2> uv = a[p_layer == 0 ? Mesh::ARRAY_TEX_UV : Mesh::ARRAY_TEX_UV2];
		if (uv.is_empty()) {
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

void MeshEditorUVTools::create_uv2(Node3D *p_node, Ref<Mesh> p_mesh) {
	Ref<Mesh> mesh2 = p_mesh;
	if (mesh2.is_null()) {
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
		if (array_mesh.is_null()) {
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

		Error err = unwrapped_mesh->lightmap_unwrap(p_node->get_global_transform());
		if (err != OK) {
			err_dialog->set_text(TTR("UV Unwrap failed, mesh may not be manifold?"));
			err_dialog->popup_centered();
			return;
		}

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Unwrap UV2"));

		ur->add_do_method(p_node, "set_mesh", unwrapped_mesh);
		ur->add_do_reference(p_node);
		ur->add_do_reference(array_mesh.ptr());

		ur->add_undo_method(p_node, "set_mesh", array_mesh);
		ur->add_undo_reference(unwrapped_mesh.ptr());

		ur->commit_action();
	}
}

void MeshEditorUVTools::debug_uv_draw() {
	if (uv_lines.is_empty()) {
		return;
	}

	debug_uv->set_clip_contents(true);
	debug_uv->draw_rect(
			Rect2(Vector2(), debug_uv->get_size()),
			get_theme_color(SNAME("dark_color_3"), EditorStringName(Editor)));

	Color mono_color = get_theme_color(SNAME("mono_color"), EditorStringName(Editor));
	// Draw an outline to represent the UV2's beginning and end area (useful on Black OLED theme).
	// Top-left coordinate needs to be `(1, 1)` to prevent `clip_contents` from clipping the top and left lines.
	debug_uv->draw_rect(
			Rect2(Vector2(1, 1), debug_uv->get_size() - Vector2(1, 1)),
			mono_color * Color(1, 1, 1, 0.125),
			false,
			Math::round(EDSCALE));

	for (int x = 1; x <= 7; x++) {
		debug_uv->draw_line(
				Vector2(debug_uv->get_size().x * 0.125 * x, 0),
				Vector2(debug_uv->get_size().x * 0.125 * x, debug_uv->get_size().y),
				mono_color * Color(1, 1, 1, 0.125),
				Math::round(EDSCALE));
	}

	for (int y = 1; y <= 7; y++) {
		debug_uv->draw_line(
				Vector2(0, debug_uv->get_size().y * 0.125 * y),
				Vector2(debug_uv->get_size().x, debug_uv->get_size().y * 0.125 * y),
				mono_color * Color(1, 1, 1, 0.125),
				Math::round(EDSCALE));
	}

	debug_uv->draw_set_transform(Vector2(), 0, debug_uv->get_size());

	// Use a translucent color to allow overlapping triangles to be visible.
	// Divide line width by the drawing scale set above, so that line width is consistent regardless of dialog size.
	// Aspect ratio is preserved by the parent AspectRatioContainer, so we only need to check the X size which is always equal to Y.
	debug_uv->draw_multiline(
			uv_lines,
			mono_color * Color(1, 1, 1, 0.5),
			Math::round(EDSCALE) / debug_uv->get_size().x);
}

MeshEditorUVTools::MeshEditorUVTools() {
	err_dialog = memnew(AcceptDialog);
	add_child(err_dialog);

	debug_uv_dialog = memnew(AcceptDialog);
	debug_uv_dialog->set_title(TTR("UV Channel Debug"));
	add_child(debug_uv_dialog);

	debug_uv_arc = memnew(AspectRatioContainer);
	debug_uv_dialog->add_child(debug_uv_arc);

	debug_uv = memnew(Control);
	debug_uv->set_custom_minimum_size(Size2(600, 600) * EDSCALE);
	debug_uv->connect(SceneStringName(draw), callable_mp(this, &MeshEditorUVTools::debug_uv_draw));
	debug_uv_arc->add_child(debug_uv);
}
