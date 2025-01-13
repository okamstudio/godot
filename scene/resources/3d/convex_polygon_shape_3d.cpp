/**************************************************************************/
/*  convex_polygon_shape_3d.cpp                                           */
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

#include "convex_polygon_shape_3d.h"
#include "core/math/convex_hull.h"
#include "scene/resources/mesh.h"
#include "servers/physics_server_3d.h"

Vector<Vector3> ConvexPolygonShape3D::get_debug_mesh_lines() const {
	Vector<Vector3> poly_points = get_points();

	if (poly_points.size() > 1) { // Need at least 2 points for a line.
		Vector<Vector3> varr = Variant(poly_points);
		Geometry3D::MeshData md;
		Error err = ConvexHullComputer::convex_hull(varr, md);
		if (err == OK) {
			Vector<Vector3> lines;
			lines.resize(md.edges.size() * 2);
			for (uint32_t i = 0; i < md.edges.size(); i++) {
				lines.write[i * 2 + 0] = md.vertices[md.edges[i].vertex_a];
				lines.write[i * 2 + 1] = md.vertices[md.edges[i].vertex_b];
			}
			return lines;
		}
	}

	return Vector<Vector3>();
}

Ref<ArrayMesh> ConvexPolygonShape3D::get_debug_arraymesh_faces(const Color &p_modulate) const {
	const Vector<Vector3> hull_points = get_points();

	Vector<Vector3> verts;
	Vector<Color> colors;
	Vector<int> indices;

	if (hull_points.size() >= 3) {
		Geometry3D::MeshData md;
		Error err = ConvexHullComputer::convex_hull(hull_points, md);
		if (err == OK) {
			verts = md.vertices;
			for (int i = 0; i < verts.size(); i++) {
				colors.push_back(p_modulate);
			}
			for (const Geometry3D::MeshData::Face &face : md.faces) {
				const int first_point = face.indices[0];
				const int indices_count = face.indices.size();
				for (int i = 1; i < indices_count - 1; i++) {
					indices.push_back(first_point);
					indices.push_back(face.indices[i]);
					indices.push_back(face.indices[i + 1]);
				}
			}
		}
	}

	Ref<ArrayMesh> mesh = memnew(ArrayMesh);
	Array a;
	a.resize(Mesh::ARRAY_MAX);
	a[RS::ARRAY_VERTEX] = verts;
	a[RS::ARRAY_COLOR] = colors;
	a[RS::ARRAY_INDEX] = indices;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, a);

	return mesh;
}

real_t ConvexPolygonShape3D::get_enclosing_radius() const {
	Vector<Vector3> data = get_points();
	const Vector3 *read = data.ptr();
	real_t r = 0.0;
	for (int i(0); i < data.size(); i++) {
		r = MAX(read[i].length_squared(), r);
	}
	return Math::sqrt(r);
}

void ConvexPolygonShape3D::_update_shape() {
	PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), points);
	Shape3D::_update_shape();
}

void ConvexPolygonShape3D::set_points(const Vector<Vector3> &p_points) {
	points = p_points;
	_update_shape();
	emit_changed();
}

Vector<Vector3> ConvexPolygonShape3D::get_points() const {
	return points;
}

void ConvexPolygonShape3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_points", "points"), &ConvexPolygonShape3D::set_points);
	ClassDB::bind_method(D_METHOD("get_points"), &ConvexPolygonShape3D::get_points);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "points"), "set_points", "get_points");
}

ConvexPolygonShape3D::ConvexPolygonShape3D() :
		Shape3D(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_CONVEX_POLYGON)) {
}
