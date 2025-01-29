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

#include "core/math/geometry_2d.h"
#include "scene/3d/physics/collision_polygon_3d.h"

CollisionPolygon3DGizmoPlugin::CollisionPolygon3DGizmoPlugin() {
	create_collision_material("shape_material", 2.0);
	create_collision_material("shape_material_arraymesh", 0.0625);

	create_collision_material("shape_material_disabled", 0.0625);
	create_collision_material("shape_material_arraymesh_disabled", 0.015625);
}

void CollisionPolygon3DGizmoPlugin::create_collision_material(const String &p_name, float p_alpha) {
	Vector<Ref<StandardMaterial3D>> mats;

	const Color collision_color(1.0, 1.0, 1.0, p_alpha);

	for (int i = 0; i < 4; i++) {
		bool instantiated = i < 2;

		Ref<StandardMaterial3D> material;
		material.instantiate();

		Color color = collision_color;
		color.a *= instantiated ? 0.25 : 1.0;

		material->set_albedo(color);
		material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
		material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN + 1);
		material->set_cull_mode(StandardMaterial3D::CULL_BACK);
		material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		material->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);

		mats.push_back(material);
	}

	materials[p_name] = mats;
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

	const Ref<StandardMaterial3D> material =
			get_material(!polygon->is_disabled() ? "shape_material" : "shape_material_disabled", p_gizmo);
	const Ref<StandardMaterial3D> material_arraymesh =
			get_material(!polygon->is_disabled() ? "shape_material_arraymesh" : "shape_material_arraymesh_disabled", p_gizmo);

	const Color collision_color = polygon->is_disabled() ? Color(1.0, 1.0, 1.0, 0.75) : polygon->get_debug_color();

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

	if (polygon->get_debug_fill_enabled()) {
		Ref<ArrayMesh> array_mesh;
		array_mesh.instantiate();

		Vector<Vector3> verts;
		Vector<Color> colors;
		Vector<int> indices;

		// Determine orientation of the 2D polygon's vertices to determine
		// which direction to draw outer polygons.
		float signed_area = 0.0f;
		for (int i = 0; i < points.size(); i++) {
			const int j = (i + 1) % points.size();
			signed_area += points[i].x * points[j].y - points[j].x * points[i].y;
		}

		// Generate triangles for the sides of the extruded polygon.
		for (int i = 0; i < points.size(); i++) {
			verts.push_back(Vector3(points[i].x, points[i].y, depth));
			verts.push_back(Vector3(points[i].x, points[i].y, -depth));

			colors.push_back(collision_color);
			colors.push_back(collision_color);
		}

		for (int i = 0; i < verts.size(); i += 2) {
			const int j = (i + 1) % verts.size();
			const int k = (i + 2) % verts.size();
			const int l = (i + 3) % verts.size();

			indices.push_back(i);
			if (signed_area < 0) {
				indices.push_back(j);
				indices.push_back(k);
			} else {
				indices.push_back(k);
				indices.push_back(j);
			}

			indices.push_back(j);
			if (signed_area < 0) {
				indices.push_back(l);
				indices.push_back(k);
			} else {
				indices.push_back(k);
				indices.push_back(l);
			}
		}

		Vector<Vector<Vector2>> decomp = Geometry2D::decompose_polygon_in_convex(polygon->get_polygon());

		// Generate triangles for the bottom cap of the extruded polygon.
		for (int i = 0; i < decomp.size(); i++) {
			Vector<Vector3> cap_verts_bottom;
			Vector<Color> cap_colours_bottom;
			Vector<int> cap_indices_bottom;

			const int index_offset = verts.size();

			const Vector<Vector2> &convex = decomp[i];

			for (int j = 0; j < convex.size(); j++) {
				cap_verts_bottom.push_back(Vector3(convex[j].x, convex[j].y, -depth));
				cap_colours_bottom.push_back(collision_color);
			}

			if (convex.size() >= 3) {
				for (int j = 1; j < convex.size(); j++) {
					const int k = (j + 1) % convex.size();

					cap_indices_bottom.push_back(index_offset + 0);
					cap_indices_bottom.push_back(index_offset + j);
					cap_indices_bottom.push_back(index_offset + k);
				}
			}
			verts.append_array(cap_verts_bottom);
			colors.append_array(cap_colours_bottom);
			indices.append_array(cap_indices_bottom);
		}

		// Generate triangles for the top cap of the extruded polygon.
		for (int i = 0; i < decomp.size(); i++) {
			Vector<Vector3> cap_verts_top;
			Vector<Color> cap_colours_top;
			Vector<int> cap_indices_top;

			const int index_offset = verts.size();

			const Vector<Vector2> &convex = decomp[i];

			for (int j = 0; j < convex.size(); j++) {
				cap_verts_top.push_back(Vector3(convex[j].x, convex[j].y, depth));
				cap_colours_top.push_back(collision_color);
			}

			if (convex.size() >= 3) {
				for (int j = 1; j < convex.size(); j++) {
					const int k = (j + 1) % convex.size();

					cap_indices_top.push_back(index_offset + k);
					cap_indices_top.push_back(index_offset + j);
					cap_indices_top.push_back(index_offset + 0);
				}
			}
			verts.append_array(cap_verts_top);
			colors.append_array(cap_colours_top);
			indices.append_array(cap_indices_top);
		}

		Array a;
		a.resize(Mesh::ARRAY_MAX);
		a[RS::ARRAY_VERTEX] = verts;
		a[RS::ARRAY_COLOR] = colors;
		a[RS::ARRAY_INDEX] = indices;
		array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, a);

		p_gizmo->add_mesh(array_mesh, material_arraymesh);
	}

	p_gizmo->add_lines(lines, material, false, collision_color);
	p_gizmo->add_collision_segments(lines);
}
