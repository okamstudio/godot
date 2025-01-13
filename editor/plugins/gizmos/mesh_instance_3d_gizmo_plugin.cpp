/**************************************************************************/
/*  mesh_instance_3d_gizmo_plugin.cpp                                     */
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

#include "mesh_instance_3d_gizmo_plugin.h"

#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/soft_body_3d.h"
#include "scene/resources/3d/primitive_meshes.h"

MeshInstance3DGizmoPlugin::MeshInstance3DGizmoPlugin() {
}

bool MeshInstance3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<MeshInstance3D>(p_spatial) != nullptr && Object::cast_to<SoftBody3D>(p_spatial) == nullptr;
}

String MeshInstance3DGizmoPlugin::get_gizmo_name() const {
	return "MeshInstance3D";
}

int MeshInstance3DGizmoPlugin::get_priority() const {
	return -1;
}

bool MeshInstance3DGizmoPlugin::can_be_hidden() const {
	return false;
}

void MeshInstance3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	Ref<Mesh> m = mesh->get_mesh();

	if (m.is_null()) {
		return; //none
	}

	Ref<TriangleMesh> tm;

	Ref<PlaneMesh> plane_mesh = mesh->get_mesh();
	if (plane_mesh.is_valid() && (plane_mesh->get_subdivide_depth() > 0 || plane_mesh->get_subdivide_width() > 0)) {
		// PlaneMesh subdiv makes gizmo redraw very slow due to TriangleMesh BVH calculation for every face.
		// For gizmo collision this is very much unnecessary since a PlaneMesh is always flat, 2 faces is enough.
		Ref<PlaneMesh> simple_plane_mesh;
		simple_plane_mesh.instantiate();
		simple_plane_mesh->set_orientation(plane_mesh->get_orientation());
		simple_plane_mesh->set_size(plane_mesh->get_size());
		simple_plane_mesh->set_center_offset(plane_mesh->get_center_offset());
		tm = simple_plane_mesh->generate_triangle_mesh();
	} else {
		tm = m->generate_triangle_mesh();
	}

	if (tm.is_valid()) {
		p_gizmo->add_collision_triangles(tm);
	}
}
