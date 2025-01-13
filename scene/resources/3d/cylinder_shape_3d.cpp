/**************************************************************************/
/*  cylinder_shape_3d.cpp                                                 */
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

#include "cylinder_shape_3d.h"

#include "scene/resources/3d/primitive_meshes.h"
#include "servers/physics_server_3d.h"

Vector<Vector3> CylinderShape3D::get_debug_mesh_lines() const {
	float c_radius = get_radius();
	float c_height = get_height();

	Vector<Vector3> points;

	Vector3 d(0, c_height * 0.5, 0);
	for (int i = 0; i < 360; i++) {
		float ra = Math::deg_to_rad((float)i);
		float rb = Math::deg_to_rad((float)i + 1);
		Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * c_radius;
		Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * c_radius;

		points.push_back(Vector3(a.x, 0, a.y) + d);
		points.push_back(Vector3(b.x, 0, b.y) + d);

		points.push_back(Vector3(a.x, 0, a.y) - d);
		points.push_back(Vector3(b.x, 0, b.y) - d);

		if (i % 90 == 0) {
			points.push_back(Vector3(a.x, 0, a.y) + d);
			points.push_back(Vector3(a.x, 0, a.y) - d);
		}
	}

	return points;
}

Ref<ArrayMesh> CylinderShape3D::get_debug_arraymesh_faces(const Color &p_modulate) const {
	Array cylinder_array;
	cylinder_array.resize(RS::ARRAY_MAX);
	CylinderMesh::create_mesh_array(cylinder_array, radius, radius, height, 32);

	Vector<Color> colors;
	const PackedVector3Array &verts = cylinder_array[RS::ARRAY_VERTEX];
	const int32_t verts_size = verts.size();
	for (int i = 0; i < verts_size; i++) {
		colors.append(p_modulate);
	}

	Ref<ArrayMesh> cylinder_mesh = memnew(ArrayMesh);
	cylinder_array[RS::ARRAY_COLOR] = colors;
	cylinder_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array);
	return cylinder_mesh;
}

real_t CylinderShape3D::get_enclosing_radius() const {
	return Vector2(radius, height * 0.5).length();
}

void CylinderShape3D::_update_shape() {
	Dictionary d;
	d["radius"] = radius;
	d["height"] = height;
	PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), d);
	Shape3D::_update_shape();
}

void CylinderShape3D::set_radius(float p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0, "CylinderShape3D radius cannot be negative.");
	radius = p_radius;
	_update_shape();
	emit_changed();
}

float CylinderShape3D::get_radius() const {
	return radius;
}

void CylinderShape3D::set_height(float p_height) {
	ERR_FAIL_COND_MSG(p_height < 0, "CylinderShape3D height cannot be negative.");
	height = p_height;
	_update_shape();
	emit_changed();
}

float CylinderShape3D::get_height() const {
	return height;
}

void CylinderShape3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &CylinderShape3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &CylinderShape3D::get_radius);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &CylinderShape3D::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &CylinderShape3D::get_height);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.001,100,0.001,or_greater,suffix:m"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,100,0.001,or_greater,suffix:m"), "set_radius", "get_radius");
}

CylinderShape3D::CylinderShape3D() :
		Shape3D(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_CYLINDER)) {
	_update_shape();
}
