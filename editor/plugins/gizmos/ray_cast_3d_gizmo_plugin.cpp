/**************************************************************************/
/*  ray_cast_3d_gizmo_plugin.cpp                                          */
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

#include "ray_cast_3d_gizmo_plugin.h"

#include "scene/3d/physics/ray_cast_3d.h"

RayCast3DGizmoPlugin::RayCast3DGizmoPlugin() {
	const Color gizmo_color = SceneTree::get_singleton()->get_debug_collisions_color();
	create_material("shape_material", gizmo_color);
	const float gizmo_value = gizmo_color.get_v();
	const Color gizmo_color_disabled = Color(gizmo_value, gizmo_value, gizmo_value, 0.65);
	create_material("shape_material_disabled", gizmo_color_disabled);
}

bool RayCast3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<RayCast3D>(p_spatial) != nullptr;
}

String RayCast3DGizmoPlugin::get_gizmo_name() const {
	return "RayCast3D";
}

int RayCast3DGizmoPlugin::get_priority() const {
	return -1;
}

void RayCast3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	RayCast3D *raycast = Object::cast_to<RayCast3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	const Ref<StandardMaterial3D> material = raycast->is_enabled() ? raycast->get_debug_material() : get_material("shape_material_disabled");

	p_gizmo->add_lines(raycast->get_debug_line_vertices(), material);

	if (raycast->get_debug_shape_thickness() > 1) {
		p_gizmo->add_vertices(raycast->get_debug_shape_vertices(), material, Mesh::PRIMITIVE_TRIANGLE_STRIP);
	}

	p_gizmo->add_collision_segments(raycast->get_debug_line_vertices());
}
