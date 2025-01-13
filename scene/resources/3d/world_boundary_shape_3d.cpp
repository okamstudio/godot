/**************************************************************************/
/*  world_boundary_shape_3d.cpp                                           */
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

#include "world_boundary_shape_3d.h"

#include "scene/resources/mesh.h"
#include "servers/physics_server_3d.h"

Vector<Vector3> WorldBoundaryShape3D::get_debug_mesh_lines() const {
	Plane p = get_plane();

	Vector3 n1 = p.get_any_perpendicular_normal();
	Vector3 n2 = p.normal.cross(n1).normalized();

	Vector3 pface[4] = {
		p.normal * p.d + n1 * 10.0 + n2 * 10.0,
		p.normal * p.d + n1 * 10.0 + n2 * -10.0,
		p.normal * p.d + n1 * -10.0 + n2 * -10.0,
		p.normal * p.d + n1 * -10.0 + n2 * 10.0,
	};

	Vector<Vector3> points = {
		pface[0],
		pface[1],
		pface[1],
		pface[2],
		pface[2],
		pface[3],
		pface[3],
		pface[0],
		p.normal * p.d,
		p.normal * p.d + p.normal * 3
	};

	return points;
}

Ref<ArrayMesh> WorldBoundaryShape3D::get_debug_arraymesh_faces(const Color &p_modulate) const {
	Plane p = get_plane();

	Vector3 n1 = p.get_any_perpendicular_normal();
	Vector3 n2 = p.normal.cross(n1).normalized();

	Vector3 pface[4] = {
		p.normal * p.d + n1 * 10.0 + n2 * 10.0,
		p.normal * p.d + n1 * 10.0 + n2 * -10.0,
		p.normal * p.d + n1 * -10.0 + n2 * -10.0,
		p.normal * p.d + n1 * -10.0 + n2 * 10.0,
	};

	Vector<Vector3> points = {
		pface[0],
		pface[1],
		pface[2],
		pface[3],
	};

	Vector<Color> colors = {
		p_modulate,
		p_modulate,
		p_modulate,
		p_modulate,
	};

	Vector<int> indices = {
		0,
		1,
		2,
		0,
		2,
		3,
	};

	Ref<ArrayMesh> mesh = memnew(ArrayMesh);
	Array a;
	a.resize(Mesh::ARRAY_MAX);
	a[RS::ARRAY_VERTEX] = points;
	a[RS::ARRAY_COLOR] = colors;
	a[RS::ARRAY_INDEX] = indices;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, a);

	return mesh;
}

void WorldBoundaryShape3D::_update_shape() {
	PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), plane);
	Shape3D::_update_shape();
}

void WorldBoundaryShape3D::set_plane(const Plane &p_plane) {
	plane = p_plane;
	_update_shape();
	emit_changed();
}

const Plane &WorldBoundaryShape3D::get_plane() const {
	return plane;
}

void WorldBoundaryShape3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_plane", "plane"), &WorldBoundaryShape3D::set_plane);
	ClassDB::bind_method(D_METHOD("get_plane"), &WorldBoundaryShape3D::get_plane);

	ADD_PROPERTY(PropertyInfo(Variant::PLANE, "plane", PROPERTY_HINT_NONE, "suffix:m"), "set_plane", "get_plane");
}

WorldBoundaryShape3D::WorldBoundaryShape3D() :
		Shape3D(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_WORLD_BOUNDARY)) {
	set_plane(Plane(0, 1, 0, 0));
}
