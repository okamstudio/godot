/**************************************************************************/
/*  mesh_instance_2d_editor_plugin.cpp                                    */
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

#include "mesh_instance_2d_editor_plugin.h"

#include "canvas_item_editor_plugin.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/multi_node_edit.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/aspect_ratio_container.h"
#include "scene/gui/box_container.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/menu_button.h"

void MeshInstance2DEditor::_node_removed(Node *p_node) {
	if (p_node == node) {
		node = nullptr;
		options->hide();
	}
}

void MeshInstance2DEditor::edit(MeshInstance2D *p_mesh) {
	node = p_mesh;
}

void MeshInstance2DEditor::_menu_option(int p_option) {
	Ref<Mesh> mesh = node->get_mesh();
	if (mesh.is_null()) {
		err_dialog->set_text(TTR("Mesh is empty!"));
		err_dialog->popup_centered();
		return;
	}

	switch (p_option) {
		case MENU_OPTION_DEBUG_UV1: {
			Ref<Mesh> mesh2 = node->get_mesh();
			if (!mesh2.is_valid()) {
				err_dialog->set_text(TTR("No mesh to debug."));
				err_dialog->popup_centered();
				return;
			}
			_create_uv_lines(0);
		} break;
	}
}

struct MeshInstance2DEditorEdgeSort {
	Vector2 a;
	Vector2 b;

	static uint32_t hash(const MeshInstance2DEditorEdgeSort &p_edge) {
		uint32_t h = hash_murmur3_one_32(HashMapHasherDefault::hash(p_edge.a));
		return hash_fmix32(hash_murmur3_one_32(HashMapHasherDefault::hash(p_edge.b), h));
	}

	bool operator==(const MeshInstance2DEditorEdgeSort &p_b) const {
		return a == p_b.a && b == p_b.b;
	}

	MeshInstance2DEditorEdgeSort() {}
	MeshInstance2DEditorEdgeSort(const Vector2 &p_a, const Vector2 &p_b) {
		if (p_a < p_b) {
			a = p_a;
			b = p_b;
		} else {
			b = p_a;
			a = p_b;
		}
	}
};

void MeshInstance2DEditor::_create_uv_lines(int p_layer) {
	Ref<Mesh> mesh = node->get_mesh();
	ERR_FAIL_COND(!mesh.is_valid());

	HashSet<MeshInstance2DEditorEdgeSort, MeshInstance2DEditorEdgeSort> edges;
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
				MeshInstance2DEditorEdgeSort edge;
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

void MeshInstance2DEditor::_debug_uv_draw() {
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

void MeshInstance2DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			options->set_button_icon(get_editor_theme_icon(SNAME("MeshInstance2D")));
		} break;
	}
}

MeshInstance2DEditor::MeshInstance2DEditor() {
	options = memnew(MenuButton);
	options->set_text(TTR("Mesh"));
	options->set_switch_on_hover(true);
	CanvasItemEditor::get_singleton()->add_control_to_menu_panel(options);

	options->get_popup()->add_item(TTR("View UV1"), MENU_OPTION_DEBUG_UV1);

	options->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &MeshInstance2DEditor::_menu_option));

	err_dialog = memnew(AcceptDialog);
	add_child(err_dialog);

	debug_uv_dialog = memnew(AcceptDialog);
	debug_uv_dialog->set_title(TTR("UV Channel Debug"));
	add_child(debug_uv_dialog);

	debug_uv_arc = memnew(AspectRatioContainer);
	debug_uv_dialog->add_child(debug_uv_arc);

	debug_uv = memnew(Control);
	debug_uv->set_custom_minimum_size(Size2(600, 600) * EDSCALE);
	debug_uv->connect(SceneStringName(draw), callable_mp(this, &MeshInstance2DEditor::_debug_uv_draw));
	debug_uv_arc->add_child(debug_uv);
}

void MeshInstance2DEditorPlugin::edit(Object *p_object) {
	{
		MeshInstance2D *mi = Object::cast_to<MeshInstance2D>(p_object);
		if (mi) {
			mesh_editor->edit(mi);
			return;
		}
	}

	Ref<MultiNodeEdit> mne = Ref<MultiNodeEdit>(p_object);
	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
	if (mne.is_valid() && edited_scene) {
		for (int i = 0; i < mne->get_node_count(); i++) {
			MeshInstance2D *mi = Object::cast_to<MeshInstance2D>(edited_scene->get_node(mne->get_node(i)));
			if (mi) {
				mesh_editor->edit(mi);
				return;
			}
		}
	}
	mesh_editor->edit(nullptr);
}

bool MeshInstance2DEditorPlugin::handles(Object *p_object) const {
	if (Object::cast_to<MeshInstance2D>(p_object)) {
		return true;
	}

	Ref<MultiNodeEdit> mne = Ref<MultiNodeEdit>(p_object);
	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
	if (mne.is_valid() && edited_scene) {
		bool has_mesh = false;
		for (int i = 0; i < mne->get_node_count(); i++) {
			if (Object::cast_to<MeshInstance2D>(edited_scene->get_node(mne->get_node(i)))) {
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

void MeshInstance2DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		mesh_editor->options->show();
	} else {
		mesh_editor->options->hide();
		mesh_editor->edit(nullptr);
	}
}

MeshInstance2DEditorPlugin::MeshInstance2DEditorPlugin() {
	mesh_editor = memnew(MeshInstance2DEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(mesh_editor);

	mesh_editor->options->hide();
}

MeshInstance2DEditorPlugin::~MeshInstance2DEditorPlugin() {
}
