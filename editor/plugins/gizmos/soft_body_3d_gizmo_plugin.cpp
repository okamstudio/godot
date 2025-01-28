/**************************************************************************/
/*  soft_body_3d_gizmo_plugin.cpp                                         */
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

#include "soft_body_3d_gizmo_plugin.h"

#include "scene/3d/soft_body_3d.h"

SoftBody3DGizmoPlugin::SoftBody3DGizmoPlugin() {
	Color gizmo_color = SceneTree::get_singleton()->get_debug_collisions_color();
	create_material("shape_material", gizmo_color);
	create_handle_material("handles");
}

bool SoftBody3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<SoftBody3D>(p_spatial) != nullptr;
}

String SoftBody3DGizmoPlugin::get_gizmo_name() const {
	return "SoftBody3D";
}

int SoftBody3DGizmoPlugin::get_priority() const {
	return -1;
}

bool SoftBody3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

void SoftBody3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	SoftBody3D *soft_body = Object::cast_to<SoftBody3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	if (!soft_body || soft_body->get_mesh().is_null()) {
		return;
	}

	// find mesh

	Vector<Vector3> lines;

	soft_body->get_mesh()->generate_debug_mesh_lines(lines);

	if (!lines.size()) {
		return;
	}

	Ref<TriangleMesh> tm = soft_body->get_mesh()->generate_triangle_mesh();

	Vector<Vector3> points;
	for (int i = 0; i < soft_body->get_mesh()->get_surface_count(); i++) {
		Array arrays = soft_body->get_mesh()->surface_get_arrays(i);
		ERR_CONTINUE(arrays.is_empty());

		const Vector<Vector3> &vertices = arrays[Mesh::ARRAY_VERTEX];
		points.append_array(vertices);
	}

	Ref<Material> material = get_material("shape_material", p_gizmo);

	p_gizmo->add_lines(lines, material);
	p_gizmo->add_handles(points, get_material("handles"));
	p_gizmo->add_collision_triangles(tm);
}

String SoftBody3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return "SoftBody3D pin point";
}

Variant SoftBody3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	SoftBody3D *soft_body = Object::cast_to<SoftBody3D>(p_gizmo->get_node_3d());
	return Variant(soft_body->is_point_pinned(p_id));
}

void SoftBody3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	SoftBody3D *soft_body = Object::cast_to<SoftBody3D>(p_gizmo->get_node_3d());
	soft_body->pin_point_toggle(p_id);
}

bool SoftBody3DGizmoPlugin::is_handle_highlighted(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	SoftBody3D *soft_body = Object::cast_to<SoftBody3D>(p_gizmo->get_node_3d());
	return soft_body->is_point_pinned(p_id);
}
