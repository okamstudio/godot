/**************************************************************************/
/*  box_shape_3d.cpp                                                      */
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

#include "box_shape_3d.h"

#include "scene/resources/3d/primitive_meshes.h"
#include "servers/physics_server_3d.h"

Vector<Vector3> BoxShape3D::get_debug_mesh_lines() const {
	Vector<Vector3> lines;
	AABB aabb;
	aabb.position = -size / 2;
	aabb.size = size;

	for (int i = 0; i < 12; i++) {
		Vector3 a, b;
		aabb.get_edge(i, a, b);
		lines.push_back(a);
		lines.push_back(b);
	}

	return lines;
}

Ref<ArrayMesh> BoxShape3D::get_debug_arraymesh_faces(const Color &p_modulate) const {
	Array box_array;
	box_array.resize(RS::ARRAY_MAX);
	BoxMesh::create_mesh_array(box_array, size);

	Vector<Color> colors;
	const PackedVector3Array &verts = box_array[RS::ARRAY_VERTEX];
	const int32_t verts_size = verts.size();
	for (int i = 0; i < verts_size; i++) {
		colors.append(p_modulate);
	}

	Ref<ArrayMesh> box_mesh = memnew(ArrayMesh);
	box_array[RS::ARRAY_COLOR] = colors;
	box_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, box_array);
	return box_mesh;
}

real_t BoxShape3D::get_enclosing_radius() const {
	return size.length() / 2;
}

void BoxShape3D::_update_shape() {
	PhysicsServer3D::get_singleton()->shape_set_data(get_shape(), size / 2);
	Shape3D::_update_shape();
}

#ifndef DISABLE_DEPRECATED
bool BoxShape3D::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "extents") { // Compatibility with Godot 3.x.
		// Convert to `size`, twice as big.
		set_size((Vector3)p_value * 2);
		return true;
	}
	return false;
}

bool BoxShape3D::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == "extents") { // Compatibility with Godot 3.x.
		// Convert to `extents`, half as big.
		r_property = size / 2;
		return true;
	}
	return false;
}
#endif // DISABLE_DEPRECATED

void BoxShape3D::set_size(const Vector3 &p_size) {
	ERR_FAIL_COND_MSG(p_size.x < 0 || p_size.y < 0 || p_size.z < 0, "BoxShape3D size cannot be negative.");
	size = p_size;
	_update_shape();
	emit_changed();
}

Vector3 BoxShape3D::get_size() const {
	return size;
}

void BoxShape3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &BoxShape3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &BoxShape3D::get_size);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
}

BoxShape3D::BoxShape3D() :
		Shape3D(PhysicsServer3D::get_singleton()->shape_create(PhysicsServer3D::SHAPE_BOX)) {
	set_size(Vector3(1, 1, 1));
}
