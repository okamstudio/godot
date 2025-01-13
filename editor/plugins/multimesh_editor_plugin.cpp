/**************************************************************************/
/*  multimesh_editor_plugin.cpp                                           */
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

#include "multimesh_editor_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/scene_tree_editor.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/option_button.h"

void MultiMeshEditor::_node_removed(Node *p_node) {
	if (p_node == node) {
		node = nullptr;
		hide();
	}
}

void MultiMeshEditor::_populate() {
	if (!node) {
		return;
	}

	Ref<Mesh> mesh;

	if (mesh_source->get_text().is_empty()) {
		Ref<MultiMesh> multimesh;
		multimesh = node->get_multimesh();
		if (multimesh.is_null()) {
			err_dialog->set_text(TTR("No mesh source specified (and no MultiMesh set in node)."));
			err_dialog->popup_centered();
			return;
		}
		if (multimesh->get_mesh().is_null()) {
			err_dialog->set_text(TTR("No mesh source specified (and MultiMesh contains no Mesh)."));
			err_dialog->popup_centered();
			return;
		}

		mesh = multimesh->get_mesh();
	} else {
		Node *ms_node = node->get_node(mesh_source->get_text());

		if (!ms_node) {
			err_dialog->set_text(TTR("Mesh source is invalid (invalid path)."));
			err_dialog->popup_centered();
			return;
		}

		MeshInstance3D *ms_instance = Object::cast_to<MeshInstance3D>(ms_node);

		if (!ms_instance) {
			err_dialog->set_text(TTR("Mesh source is invalid (not a MeshInstance3D)."));
			err_dialog->popup_centered();
			return;
		}

		mesh = ms_instance->get_mesh();

		if (mesh.is_null()) {
			err_dialog->set_text(TTR("Mesh source is invalid (contains no Mesh resource)."));
			err_dialog->popup_centered();
			return;
		}
	}

	if (surface_source->get_text().is_empty()) {
		err_dialog->set_text(TTR("No surface source specified."));
		err_dialog->popup_centered();
		return;
	}

	Node *ss_node = node->get_node(surface_source->get_text());

	if (!ss_node) {
		err_dialog->set_text(TTR("Surface source is invalid (invalid path)."));
		err_dialog->popup_centered();
		return;
	}

	MeshInstance3D *ss_instance = Object::cast_to<MeshInstance3D>(ss_node);

	if (!ss_instance || ss_instance->get_mesh().is_null()) {
		err_dialog->set_text(TTR("Surface source is invalid (no geometry)."));
		err_dialog->popup_centered();
		return;
	}

	Transform3D geom_xform = node->get_global_transform().affine_inverse() * ss_instance->get_global_transform();

	Vector<Face3> geometry = ss_instance->get_mesh()->get_faces();

	if (geometry.size() == 0) {
		err_dialog->set_text(TTR("Surface source is invalid (no faces)."));
		err_dialog->popup_centered();
		return;
	}

	//make all faces local

	int gc = geometry.size();
	Face3 *w = geometry.ptrw();

	for (int i = 0; i < gc; i++) {
		for (int j = 0; j < 3; j++) {
			w[i].vertex[j] = geom_xform.xform(w[i].vertex[j]);
		}
	}

	Vector<Face3> faces = geometry;
	int facecount = faces.size();
	ERR_FAIL_COND_MSG(!facecount, "Parent has no solid faces to populate.");

	const Face3 *r = faces.ptr();

	float area_accum = 0;
	RBMap<float, int> triangle_area_map;
	for (int i = 0; i < facecount; i++) {
		float area = r[i].get_area();
		if (area < CMP_EPSILON) {
			continue;
		}
		triangle_area_map[area_accum] = i;
		area_accum += area;
	}

	ERR_FAIL_COND_MSG(triangle_area_map.is_empty(), "Couldn't map area.");
	ERR_FAIL_COND_MSG(area_accum == 0, "Couldn't map area.");

	Ref<MultiMesh> multimesh = memnew(MultiMesh);
	multimesh->set_mesh(mesh);

	int instance_count = populate_amount->get_value();

	multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	multimesh->set_use_colors(false);
	multimesh->set_instance_count(instance_count);

	float _tilt_random = populate_tilt_random->get_value();
	float _rotate_random = populate_rotate_random->get_value();
	float _scale_random = populate_scale_random->get_value();
	float _scale = populate_scale->get_value();
	int axis = populate_axis->get_selected();

	Transform3D axis_xform;
	if (axis == Vector3::AXIS_Z) {
		axis_xform.rotate(Vector3(1, 0, 0), -Math_PI * 0.5);
	}
	if (axis == Vector3::AXIS_X) {
		axis_xform.rotate(Vector3(0, 0, 1), -Math_PI * 0.5);
	}

	for (int i = 0; i < instance_count; i++) {
		float areapos = Math::random(0.0f, area_accum);

		RBMap<float, int>::Iterator E = triangle_area_map.find_closest(areapos);
		ERR_FAIL_COND(!E);
		int index = E->value;
		ERR_FAIL_INDEX(index, facecount);

		// ok FINALLY get face
		Face3 face = r[index];
		//now compute some position inside the face...

		Vector3 pos = face.get_random_point_inside();
		Vector3 normal = face.get_plane().normal;
		Vector3 op_axis = (face.vertex[0] - face.vertex[1]).normalized();

		Transform3D xform;

		xform.set_look_at(pos, pos + op_axis, normal);
		xform = xform * axis_xform;

		Basis post_xform;

		post_xform.rotate(xform.basis.get_column(1), -Math::random(-_rotate_random, _rotate_random) * Math_PI);
		post_xform.rotate(xform.basis.get_column(2), -Math::random(-_tilt_random, _tilt_random) * Math_PI);
		post_xform.rotate(xform.basis.get_column(0), -Math::random(-_tilt_random, _tilt_random) * Math_PI);

		xform.basis = post_xform * xform.basis;
		//xform.basis.orthonormalize();

		xform.basis.scale(Vector3(1, 1, 1) * (_scale + Math::random(-_scale_random, _scale_random)));

		multimesh->set_instance_transform(i, xform);
	}

	node->set_multimesh(multimesh);
}

void MultiMeshEditor::_browsed(const NodePath &p_path) {
	NodePath path = node->get_path_to(get_node(p_path));

	if (browsing_source) {
		mesh_source->set_text(path);
	} else {
		surface_source->set_text(path);
	}
}

void MultiMeshEditor::_menu_option(int p_option) {
	switch (p_option) {
		case MENU_OPTION_POPULATE: {
			if (_last_pp_node != node) {
				surface_source->set_text("..");
				mesh_source->set_text("..");
				populate_axis->select(1);
				populate_rotate_random->set_value(0);
				populate_tilt_random->set_value(0);
				populate_scale_random->set_value(0);
				populate_scale->set_value(1);
				populate_amount->set_value(128);

				_last_pp_node = node;
			}
			populate_dialog->popup_centered(Size2(250, 380));

		} break;
	}
}

void MultiMeshEditor::edit(MultiMeshInstance3D *p_multimesh) {
	node = p_multimesh;
}

void MultiMeshEditor::_browse(bool p_source) {
	browsing_source = p_source;
	Node *browsed_node = nullptr;
	if (p_source) {
		browsed_node = node->get_node_or_null(mesh_source->get_text());
		std->set_title(TTR("Select a Source Mesh:"));
	} else {
		browsed_node = node->get_node_or_null(surface_source->get_text());
		std->set_title(TTR("Select a Target Surface:"));
	}
	std->popup_scenetree_dialog(browsed_node);
}

MultiMeshEditor::MultiMeshEditor() {
	options = memnew(MenuButton);
	options->set_switch_on_hover(true);
	Node3DEditor::get_singleton()->add_control_to_menu_panel(options);

	options->set_text("MultiMesh");
	options->set_button_icon(EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("MultiMeshInstance3D"), EditorStringName(EditorIcons)));

	options->get_popup()->add_item(TTR("Populate Surface"));
	options->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &MultiMeshEditor::_menu_option));

	populate_dialog = memnew(ConfirmationDialog);
	populate_dialog->set_title(TTR("Populate MultiMesh"));
	add_child(populate_dialog);

	VBoxContainer *vbc = memnew(VBoxContainer);
	populate_dialog->add_child(vbc);
	//populate_dialog->set_child_rect(vbc);

	HBoxContainer *hbc = memnew(HBoxContainer);

	surface_source = memnew(LineEdit);
	hbc->add_child(surface_source);
	surface_source->set_h_size_flags(SIZE_EXPAND_FILL);
	Button *b = memnew(Button);
	hbc->add_child(b);
	b->set_text("..");
	b->connect(SceneStringName(pressed), callable_mp(this, &MultiMeshEditor::_browse).bind(false));

	vbc->add_margin_child(TTR("Target Surface:"), hbc);

	hbc = memnew(HBoxContainer);
	mesh_source = memnew(LineEdit);
	hbc->add_child(mesh_source);
	mesh_source->set_h_size_flags(SIZE_EXPAND_FILL);
	b = memnew(Button);
	hbc->add_child(b);
	b->set_text("..");
	vbc->add_margin_child(TTR("Source Mesh:"), hbc);
	b->connect(SceneStringName(pressed), callable_mp(this, &MultiMeshEditor::_browse).bind(true));

	populate_axis = memnew(OptionButton);
	populate_axis->add_item(TTR("X-Axis"));
	populate_axis->add_item(TTR("Y-Axis"));
	populate_axis->add_item(TTR("Z-Axis"));
	populate_axis->select(2);
	vbc->add_margin_child(TTR("Mesh Up Axis:"), populate_axis);

	populate_rotate_random = memnew(HSlider);
	populate_rotate_random->set_max(1);
	populate_rotate_random->set_step(0.01);
	vbc->add_margin_child(TTR("Random Rotation:"), populate_rotate_random);

	populate_tilt_random = memnew(HSlider);
	populate_tilt_random->set_max(1);
	populate_tilt_random->set_step(0.01);
	vbc->add_margin_child(TTR("Random Tilt:"), populate_tilt_random);

	populate_scale_random = memnew(SpinBox);
	populate_scale_random->set_min(0);
	populate_scale_random->set_max(1);
	populate_scale_random->set_value(0);
	populate_scale_random->set_step(0.01);

	vbc->add_margin_child(TTR("Random Scale:"), populate_scale_random);

	populate_scale = memnew(SpinBox);
	populate_scale->set_min(0.001);
	populate_scale->set_max(4096);
	populate_scale->set_value(1);
	populate_scale->set_step(0.01);

	vbc->add_margin_child(TTR("Scale:"), populate_scale);

	populate_amount = memnew(SpinBox);
	populate_amount->set_anchor(SIDE_RIGHT, ANCHOR_END);
	populate_amount->set_begin(Point2(20, 232));
	populate_amount->set_end(Point2(-5, 237));
	populate_amount->set_min(1);
	populate_amount->set_max(65536);
	populate_amount->set_value(128);
	vbc->add_margin_child(TTR("Amount:"), populate_amount);

	populate_dialog->set_ok_button_text(TTR("Populate"));

	populate_dialog->get_ok_button()->connect(SceneStringName(pressed), callable_mp(this, &MultiMeshEditor::_populate));
	std = memnew(SceneTreeDialog);
	Vector<StringName> valid_types;
	valid_types.push_back("MeshInstance3D");
	std->set_valid_types(valid_types);
	populate_dialog->add_child(std);
	std->connect("selected", callable_mp(this, &MultiMeshEditor::_browsed));

	_last_pp_node = nullptr;

	err_dialog = memnew(AcceptDialog);
	add_child(err_dialog);
}

void MultiMeshEditorPlugin::edit(Object *p_object) {
	multimesh_editor->edit(Object::cast_to<MultiMeshInstance3D>(p_object));
}

bool MultiMeshEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("MultiMeshInstance3D");
}

void MultiMeshEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		multimesh_editor->options->show();
	} else {
		multimesh_editor->options->hide();
		multimesh_editor->edit(nullptr);
	}
}

MultiMeshEditorPlugin::MultiMeshEditorPlugin() {
	multimesh_editor = memnew(MultiMeshEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(multimesh_editor);

	multimesh_editor->options->hide();
}

MultiMeshEditorPlugin::~MultiMeshEditorPlugin() {
}
