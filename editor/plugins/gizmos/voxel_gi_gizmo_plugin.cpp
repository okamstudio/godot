/**************************************************************************/
/*  voxel_gi_gizmo_plugin.cpp                                             */
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

#include "voxel_gi_gizmo_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/plugins/gizmos/gizmo_3d_helper.h"
#include "scene/3d/voxel_gi.h"

VoxelGIGizmoPlugin::VoxelGIGizmoPlugin() {
	helper.instantiate();

	Color gizmo_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/voxel_gi");

	create_material("voxel_gi_material", gizmo_color);

	// This gizmo draws a lot of lines. Use a low opacity to make it not too intrusive.
	gizmo_color.a = 0.02;
	create_material("voxel_gi_internal_material", gizmo_color);

	create_icon_material("voxel_gi_icon", EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("GizmoVoxelGI"), EditorStringName(EditorIcons)));
	create_handle_material("handles");
}

VoxelGIGizmoPlugin::~VoxelGIGizmoPlugin() {
}

bool VoxelGIGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<VoxelGI>(p_spatial) != nullptr;
}

String VoxelGIGizmoPlugin::get_gizmo_name() const {
	return "VoxelGI";
}

int VoxelGIGizmoPlugin::get_priority() const {
	return -1;
}

String VoxelGIGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return helper->box_get_handle_name(p_id);
}

Variant VoxelGIGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	VoxelGI *probe = Object::cast_to<VoxelGI>(p_gizmo->get_node_3d());
	return probe->get_size();
}

void VoxelGIGizmoPlugin::begin_handle_action(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) {
	helper->initialize_handle_action(get_handle_value(p_gizmo, p_id, p_secondary), p_gizmo->get_node_3d()->get_global_transform());
}

void VoxelGIGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	VoxelGI *probe = Object::cast_to<VoxelGI>(p_gizmo->get_node_3d());

	Vector3 sg[2];
	helper->get_segment(p_camera, p_point, sg);

	Vector3 size = probe->get_size();
	Vector3 position;
	helper->box_set_handle(sg, p_id, size, position);
	probe->set_size(size);
	probe->set_global_position(position);
}

void VoxelGIGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	helper->box_commit_handle(TTR("Change Probe Size"), p_cancel, p_gizmo->get_node_3d());
}

void VoxelGIGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	p_gizmo->clear();

	if (p_gizmo->is_selected()) {
		VoxelGI *probe = Object::cast_to<VoxelGI>(p_gizmo->get_node_3d());
		Ref<Material> material = get_material("voxel_gi_material", p_gizmo);
		Ref<Material> material_internal = get_material("voxel_gi_internal_material", p_gizmo);

		Vector<Vector3> lines;
		Vector3 size = probe->get_size();

		static const int subdivs[VoxelGI::SUBDIV_MAX] = { 64, 128, 256, 512 };

		AABB aabb = AABB(-size / 2, size);
		int subdiv = subdivs[probe->get_subdiv()];
		float cell_size = aabb.get_longest_axis_size() / subdiv;

		for (int i = 0; i < 12; i++) {
			Vector3 a, b;
			aabb.get_edge(i, a, b);
			lines.push_back(a);
			lines.push_back(b);
		}

		p_gizmo->add_lines(lines, material);

		lines.clear();

		for (int i = 1; i < subdiv; i++) {
			for (int j = 0; j < 3; j++) {
				if (cell_size * i > aabb.size[j]) {
					continue;
				}

				int j_n1 = (j + 1) % 3;
				int j_n2 = (j + 2) % 3;

				for (int k = 0; k < 4; k++) {
					Vector3 from = aabb.position, to = aabb.position;
					from[j] += cell_size * i;
					to[j] += cell_size * i;

					if (k & 1) {
						to[j_n1] += aabb.size[j_n1];
					} else {
						to[j_n2] += aabb.size[j_n2];
					}

					if (k & 2) {
						from[j_n1] += aabb.size[j_n1];
						from[j_n2] += aabb.size[j_n2];
					}

					lines.push_back(from);
					lines.push_back(to);
				}
			}
		}

		p_gizmo->add_lines(lines, material_internal);

		Vector<Vector3> handles = helper->box_get_handles(probe->get_size());

		p_gizmo->add_handles(handles, get_material("handles"));
	}

	Ref<Material> icon = get_material("voxel_gi_icon", p_gizmo);
	p_gizmo->add_unscaled_billboard(icon, 0.05);
}
