/**************************************************************************/
/*  concave_polygon_shape_3d.cpp                                          */
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

#include "concave_polygon_shape_3d.h"

#include "scene/resources/mesh.h"
#include "servers/physics_server_3d.h"

Vector<Vector3> ConcavePolygonShape3D::get_debug_mesh_lines() const {
	HashSet<DrawEdge, DrawEdge> edges;

	int index_count = faces.size();
	ERR_FAIL_COND_V((index_count % 3) != 0, Vector<Vector3>());

	const Vector3 *r = faces.ptr();

	for (int i = 0; i < index_count; i += 3) {
		for (int j = 0; j < 3; j++) {
			DrawEdge de(r[i + j], r[i + ((j + 1) % 3)]);
			edges.insert(de);
		}
	}

	Vector<Vector3> points;
	points.resize(edges.size() * 2);
	int idx = 0;
	for (const DrawEdge &E : edges) {
		points.write[idx + 0] = E.a;
		points.write[idx + 1] = E.b;
		idx += 2;
	}

	return points;
}

Ref<ArrayMesh> ConcavePolygonShape3D::get_debug_arraymesh_faces(const Color &p_modulate) const {
	Vector<Color> colors;

	for (int i = 0; i < faces.size(); i++) {
		colors.push_back(p_modulate);
	}

	Ref<ArrayMesh> mesh = memnew(ArrayMesh);
	Array a;
	a.resize(Mesh::ARRAY_MAX);
	a[RS::ARRAY_VERTEX] = faces;
	a[RS::ARRAY_COLOR] = colors;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, a);

	return mesh;
}

real_t ConcavePolygonShape3D::get_enclosing_radius() const {
	Vector<Vector3> data = get_faces();
	const Vector3 *read = data.ptr();
	real_t r = 0.0;
	for (int i(0); i < data.size(); i++) {
		r = MAX(read[i].length_squared(), r);
	}
	return Math::sqrt(r);
}

void ConcavePolygonShape3D::_update_shape() {
	Dictionary d;
	d["faces"] = faces;
	d["backface_collision"] = backface_collision;
	PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), d);

	Shape3D::_update_shape();
}

void ConcavePolygonShape3D::set_faces(const Vector<Vector3> &p_faces) {
	faces = p_faces;
	_update_shape();
	emit_changed();
}

Vector<Vector3> ConcavePolygonShape3D::get_faces() const {
	return faces;
}

void ConcavePolygonShape3D::set_backface_collision_enabled(bool p_enabled) {
	backface_collision = p_enabled;

	if (!faces.is_empty()) {
		_update_shape();
		emit_changed();
	}
}

bool ConcavePolygonShape3D::is_backface_collision_enabled() const {
	return backface_collision;
}

void ConcavePolygonShape3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_faces", "faces"), &ConcavePolygonShape3D::set_faces);
	ClassDB::bind_method(D_METHOD("get_faces"), &ConcavePolygonShape3D::get_faces);

	ClassDB::bind_method(D_METHOD("set_backface_collision_enabled", "enabled"), &ConcavePolygonShape3D::set_backface_collision_enabled);
	ClassDB::bind_method(D_METHOD("is_backface_collision_enabled"), &ConcavePolygonShape3D::is_backface_collision_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_faces", "get_faces");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "backface_collision"), "set_backface_collision_enabled", "is_backface_collision_enabled");
}

ConcavePolygonShape3D::ConcavePolygonShape3D() :
		Shape3D(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_CONCAVE_POLYGON)) {
	//set_planes(Vector3(1,1,1));
}
