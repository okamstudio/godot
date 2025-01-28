/**************************************************************************/
/*  geometry_instance_3d_gizmo_plugin.cpp                                 */
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

#include "geometry_instance_3d_gizmo_plugin.h"

#include "editor/editor_settings.h"
#include "scene/3d/visual_instance_3d.h"

GeometryInstance3DGizmoPlugin::GeometryInstance3DGizmoPlugin() {
}

bool GeometryInstance3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<GeometryInstance3D>(p_spatial) != nullptr;
}

String GeometryInstance3DGizmoPlugin::get_gizmo_name() const {
	return "MeshInstance3DCustomAABB";
}

int GeometryInstance3DGizmoPlugin::get_priority() const {
	return -1;
}

void GeometryInstance3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	GeometryInstance3D *geometry = Object::cast_to<GeometryInstance3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	if (p_gizmo->is_selected()) {
		AABB aabb = geometry->get_custom_aabb();

		Vector<Vector3> lines;
		for (int i = 0; i < 12; i++) {
			Vector3 a;
			Vector3 b;
			aabb.get_edge(i, a, b);

			lines.push_back(a);
			lines.push_back(b);
		}

		Ref<StandardMaterial3D> mat = memnew(StandardMaterial3D);
		mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		const Color selection_box_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/aabb");
		mat->set_albedo(selection_box_color);
		mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
		p_gizmo->add_lines(lines, mat);
	}
}
