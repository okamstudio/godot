/**************************************************************************/
/*  collision_polygon_3d_gizmo_plugin.cpp                                 */
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

#include "collision_polygon_3d_gizmo_plugin.h"

#include "scene/3d/physics/collision_polygon_3d.h"

CollisionPolygon3DGizmoPlugin::CollisionPolygon3DGizmoPlugin() {
	const Color gizmo_color = SceneTree::get_singleton()->get_debug_collisions_color();
	create_material("shape_material", gizmo_color);
	const float gizmo_value = gizmo_color.get_v();
	const Color gizmo_color_disabled = Color(gizmo_value, gizmo_value, gizmo_value, 0.65);
	create_material("shape_material_disabled", gizmo_color_disabled);
}

bool CollisionPolygon3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<CollisionPolygon3D>(p_spatial) != nullptr;
}

String CollisionPolygon3DGizmoPlugin::get_gizmo_name() const {
	return "CollisionPolygon3D";
}

int CollisionPolygon3DGizmoPlugin::get_priority() const {
	return -1;
}

void CollisionPolygon3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	CollisionPolygon3D *polygon = Object::cast_to<CollisionPolygon3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	Vector<Vector2> points = polygon->get_polygon();
	float depth = polygon->get_depth() * 0.5;

	Vector<Vector3> lines;
	for (int i = 0; i < points.size(); i++) {
		int n = (i + 1) % points.size();
		lines.push_back(Vector3(points[i].x, points[i].y, depth));
		lines.push_back(Vector3(points[n].x, points[n].y, depth));
		lines.push_back(Vector3(points[i].x, points[i].y, -depth));
		lines.push_back(Vector3(points[n].x, points[n].y, -depth));
		lines.push_back(Vector3(points[i].x, points[i].y, depth));
		lines.push_back(Vector3(points[i].x, points[i].y, -depth));
	}

	const Ref<Material> material =
			get_material(!polygon->is_disabled() ? "shape_material" : "shape_material_disabled", p_gizmo);

	p_gizmo->add_lines(lines, material);
	p_gizmo->add_collision_segments(lines);
}
